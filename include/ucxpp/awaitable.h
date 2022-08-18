#pragma once

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>
#include <ucs/type/status.h>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/error.h"

#include "ucxpp/detail/debug.h"
struct request_context {
  void *awaitable;
};

namespace ucxpp {

class base_awaitable {
protected:
  std::atomic<std::coroutine_handle<>> h_;
  ucs_status_t status_;
  base_awaitable() : h_(nullptr), status_(UCS_OK) {}
  bool check_request_ready(ucs_status_ptr_t request) {
    status_ = UCS_PTR_STATUS(request);
    if (UCS_PTR_IS_ERR(status_)) [[unlikely]] {
      UCXPP_LOG_ERROR("%s", ::ucs_status_string(status_));
      return true;
    }

    return status_ == UCS_OK;
  }
};

/* Common awaitable class for send-like callbacks */
template <class Derived> class send_awaitable : public base_awaitable {
public:
  static void send_cb(void *request, ucs_status_t status, void *user_data) {
    auto self = static_cast<Derived *>(user_data);
    self->status_ = status;
    ::ucp_request_free(request);

    while (!self->h_.load())
      [[unlikely]] { std::this_thread::yield(); }
    self->h_.load().resume();
  }

  ucp_request_param_t build_param() {
    ucp_request_param_t send_param;
    send_param.op_attr_mask =
        UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
    send_param.cb.send = &send_cb;
    send_param.user_data = this;
    return send_param;
  }

  bool await_suspend(std::coroutine_handle<> h) {
    h_ = h;
    return true;
  }

  void await_resume() const { check_ucs_status(status_, "operation failed"); }
};

class stream_send_awaitable : public send_awaitable<stream_send_awaitable> {
  ucp_ep_h ep_;
  void const *buffer_;
  size_t length_;
  friend class send_awaitable;

public:
  stream_send_awaitable(ucp_ep_h ep, void const *buffer, size_t length)
      : ep_(ep), buffer_(buffer), length_(length) {}

  bool await_ready() noexcept {
    auto send_param = build_param();
    auto request = ::ucp_stream_send_nbx(ep_, buffer_, length_, &send_param);
    return check_request_ready(request);
  }
};

class tag_send_awaitable : public send_awaitable<tag_send_awaitable> {
  ucp_ep_h ep_;
  ucp_tag_t tag_;
  void const *buffer_;
  size_t length_;
  friend class send_awaitable;

public:
  tag_send_awaitable(ucp_ep_h ep, void const *buffer, size_t length,
                     ucp_tag_t tag)
      : ep_(ep), tag_(tag), buffer_(buffer), length_(length) {}

  bool await_ready() noexcept {
    auto send_param = build_param();
    auto request = ::ucp_tag_send_nbx(ep_, buffer_, length_, tag_, &send_param);
    return check_request_ready(request);
  }
};

class rma_put_awaitable : public send_awaitable<rma_put_awaitable> {
  ucp_ep_h ep_;
  void const *buffer_;
  size_t length_;
  uint64_t remote_addr_;
  ucp_rkey_h rkey_;
  friend class send_awaitable;

public:
  rma_put_awaitable(ucp_ep_h ep, void const *buffer, size_t length,
                    uint64_t remote_addr, ucp_rkey_h rkey)
      : ep_(ep), buffer_(buffer), length_(length), remote_addr_(remote_addr),
        rkey_(rkey) {}

  bool await_ready() noexcept {
    auto send_param = build_param();
    auto request =
        ::ucp_put_nbx(ep_, buffer_, length_, remote_addr_, rkey_, &send_param);
    return check_request_ready(request);
  }
};

class rma_get_awaitable : public send_awaitable<rma_get_awaitable> {
  ucp_ep_h ep_;
  void *buffer_;
  size_t length_;
  uint64_t remote_addr_;
  ucp_rkey_h rkey_;
  friend class send_awaitable;

public:
  rma_get_awaitable(ucp_ep_h ep, void *buffer, size_t length,
                    uint64_t remote_addr, ucp_rkey_h rkey)
      : ep_(ep), buffer_(buffer), length_(length), remote_addr_(remote_addr),
        rkey_(rkey) {}

  bool await_ready() noexcept {
    auto send_param = build_param();
    auto request =
        ::ucp_get_nbx(ep_, buffer_, length_, remote_addr_, rkey_, &send_param);
    return check_request_ready(request);
  }
};

class ep_flush_awaitable : public send_awaitable<ep_flush_awaitable> {
  ucp_ep_h ep_;
  friend class send_awaitable;

public:
  ep_flush_awaitable(ucp_ep_h ep) : ep_(ep) {}

  bool await_ready() noexcept {
    auto send_param = build_param();
    auto request = ::ucp_ep_flush_nbx(ep_, &send_param);
    return check_request_ready(request);
  }
};

class ep_close_awaitable : public send_awaitable<ep_close_awaitable> {
  ucp_ep_h ep_;
  friend class send_awaitable;

public:
  ep_close_awaitable(ucp_ep_h ep) : ep_(ep) {}

  bool await_ready() noexcept {
    auto send_param = build_param();
    auto request = ::ucp_ep_close_nbx(ep_, &send_param);
    return check_request_ready(request);
  }
};

class worker_flush_awaitable : public send_awaitable<worker_flush_awaitable> {
  ucp_worker_h worker_;
  friend class send_awaitable;

public:
  worker_flush_awaitable(ucp_worker_h worker) : worker_(worker) {}

  bool await_ready() noexcept {
    auto send_param = build_param();
    auto request = ::ucp_worker_flush_nbx(worker_, &send_param);
    return check_request_ready(request);
  }
};

/* Common awaitable class for stream-recv-like callbacks */
class stream_recv_awaitable : base_awaitable {
private:
  ucp_ep_h ep_;
  size_t received_;
  void *buffer_;
  size_t length_;

public:
  stream_recv_awaitable(ucp_ep_h ep, void *buffer, size_t length)
      : ep_(ep), received_(0), buffer_(buffer), length_(length) {}

  static void stream_recv_cb(void *request, ucs_status_t status,
                             size_t received, void *user_data) {
    auto self = reinterpret_cast<stream_recv_awaitable *>(user_data);
    self->status_ = status;
    self->received_ = received;
    ::ucp_request_free(request);
    while (!self->h_.load())
      [[unlikely]] { std::this_thread::yield(); }
    self->h_.load().resume();
  }

  bool await_ready() noexcept {
    ucp_request_param_t stream_recv_param;
    stream_recv_param.op_attr_mask =
        UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
    stream_recv_param.cb.recv_stream = &stream_recv_cb;
    stream_recv_param.user_data = this;
    auto request =
        ::ucp_stream_send_nbx(ep_, buffer_, length_, &stream_recv_param);
    return check_request_ready(request);
  }

  bool await_suspend(std::coroutine_handle<> h) {
    h_ = h;
    return true;
  }

  size_t await_resume() const {
    check_ucs_status(status_, "operation failed");
    return received_;
  }
};

/* Common awaitable class for tag-recv-like callbacks */
class tag_recv_awaitable : public base_awaitable {

private:
  ucp_worker_h worker_;
  void *buffer_;
  size_t length_;
  ucp_tag_t tag_;
  ucp_tag_t tag_mask_;
  ucp_tag_recv_info_t recv_info_;

public:
  tag_recv_awaitable(ucp_worker_h worker, void *buffer, size_t length,
                     ucp_tag_t tag, ucp_tag_t tag_mask)
      : worker_(worker), buffer_(buffer), length_(length), tag_(tag),
        tag_mask_(tag_mask) {}

  static void tag_recv_cb(void *request, ucs_status_t status,
                          ucp_tag_recv_info_t const *tag_info,
                          void *user_data) {
    auto self = reinterpret_cast<tag_recv_awaitable *>(user_data);
    self->status_ = status;
    self->recv_info_.length = tag_info->length;
    self->recv_info_.sender_tag = tag_info->sender_tag;
    ::ucp_request_free(request);
    while (!self->h_.load())
      [[unlikely]] { std::this_thread::yield(); }
    self->h_.load().resume();
  }

  bool await_ready() noexcept {
    ucp_request_param_t tag_recv_param;
    tag_recv_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                                  UCP_OP_ATTR_FIELD_USER_DATA |
                                  UCP_OP_ATTR_FIELD_RECV_INFO;
    tag_recv_param.cb.recv = &tag_recv_cb;
    tag_recv_param.user_data = this;
    tag_recv_param.recv_info.tag_info = &recv_info_;

    auto request = ::ucp_tag_recv_nbx(worker_, buffer_, length_, tag_,
                                      tag_mask_, &tag_recv_param);

    return check_request_ready(request);
  }

  bool await_suspend(std::coroutine_handle<> h) {
    h_ = h;
    return true;
  }

  std::pair<size_t, ucp_tag_t> await_resume() const {
    check_ucs_status(status_, "error in ucp_tag_recv_nbx");
    return std::make_pair(recv_info_.length, recv_info_.sender_tag);
  }
};

} // namespace ucxpp