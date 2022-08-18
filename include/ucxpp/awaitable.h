#pragma once

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ucs/type/status.h>

#include <ucp/api/ucp.h>

#include "ucxpp/error.h"
struct request_context {
  void *awaitable;
};

namespace ucxpp {

/* Common awaitable class for send-like callbacks */
template <class Derived> class send_awaitable {
  std::atomic<std::coroutine_handle<>> h_;
  ucs_status_t status_;

public:
  static void send_cb(void *request, ucs_status_t status, void *user_data) {
    auto self = static_cast<Derived *>(user_data);
    self->status_ = status;
    ::ucp_request_free(request);
    self->done_.store(true);
    if (self->h_.load()) {
      self->h_.load().resume();
    }
  }

  ucp_request_param_t build_param() {
    ucp_request_param_t send_param;
    send_param.op_attr_mask =
        UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
    send_param.cb.send = &send_cb;
    send_param.user_data = this;
    return send_param;
  }

  bool check_request_ready(ucs_status_ptr_t request) {
    status_ = UCS_PTR_STATUS(request);
    if (UCS_PTR_IS_ERR(status_)) {
      return true;
    }

    return status_ == UCS_OK;
  }

  bool await_suspend(std::coroutine_handle<> h) {
    if (reinterpret_cast<Derived *>(this)->done_) {
      return false;
    }
    h_ = h;
    return true;
  }

  void await_resume() const { check_ucs_status(status_, "operation failed"); }
};

class stream_send_awaitable : public send_awaitable<stream_send_awaitable> {
  ucp_ep_h ep_;
  void const *buffer_;
  size_t length_;
  std::atomic<bool> done_;
  friend class send_awaitable;

public:
  stream_send_awaitable(ucp_ep_h ep, void const *buffer, size_t length)
      : ep_(ep), buffer_(buffer), length_(length), done_(false) {}

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
  std::atomic<bool> done_;
  friend class send_awaitable;

public:
  tag_send_awaitable(ucp_ep_h ep, void const *buffer, size_t length,
                     ucp_tag_t tag)
      : ep_(ep), tag_(tag), buffer_(buffer), length_(length), done_(false) {}

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
  std::atomic<bool> done_;
  friend class send_awaitable;

public:
  rma_put_awaitable(ucp_ep_h ep, void const *buffer, size_t length,
                    uint64_t remote_addr, ucp_rkey_h rkey)
      : ep_(ep), buffer_(buffer), length_(length), remote_addr_(remote_addr),
        rkey_(rkey), done_(false) {}

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
  std::atomic<bool> done_;
  friend class send_awaitable;

public:
  rma_get_awaitable(ucp_ep_h ep, void *buffer, size_t length,
                    uint64_t remote_addr, ucp_rkey_h rkey)
      : ep_(ep), buffer_(buffer), length_(length), remote_addr_(remote_addr),
        rkey_(rkey), done_(false) {}

  bool await_ready() noexcept {
    auto send_param = build_param();
    auto request =
        ::ucp_get_nbx(ep_, buffer_, length_, remote_addr_, rkey_, &send_param);
    return check_request_ready(request);
  }
};

class ep_flush_awaitable : public send_awaitable<ep_flush_awaitable> {
  ucp_ep_h ep_;
  std::atomic<bool> done_;
  friend class send_awaitable;

public:
  ep_flush_awaitable(ucp_ep_h ep) : ep_(ep), done_(false) {}

  bool await_ready() noexcept {
    auto send_param = build_param();
    auto request = ::ucp_ep_flush_nbx(ep_, &send_param);
    return check_request_ready(request);
  }
};

class ep_close_awaitable : public send_awaitable<ep_close_awaitable> {
  ucp_ep_h ep_;
  std::atomic<bool> done_;
  friend class send_awaitable;

public:
  ep_close_awaitable(ucp_ep_h ep) : ep_(ep), done_(false) {}

  bool await_ready() noexcept {
    auto send_param = build_param();
    auto request = ::ucp_ep_close_nbx(ep_, &send_param);
    return check_request_ready(request);
  }
};

class worker_flush_awaitable : public send_awaitable<worker_flush_awaitable> {
  ucp_worker_h worker_;
  std::atomic<bool> done_;
  friend class send_awaitable;

public:
  worker_flush_awaitable(ucp_worker_h worker) : worker_(worker), done_(false) {}

  bool await_ready() noexcept {
    auto send_param = build_param();
    auto request = ::ucp_worker_flush_nbx(worker_, &send_param);
    return check_request_ready(request);
  }
};

/* Common awaitable class for stream-recv-like callbacks */
class stream_recv_awaitable {
public:
  using request_fn_t = std::function<ucs_status_ptr_t(
      ucp_request_param_t const *, size_t *received)>;

private:
  request_fn_t request_fn_;
  std::coroutine_handle<> h_;
  size_t received_;
  ucs_status_t status_;

public:
  stream_recv_awaitable(request_fn_t &&request_fn)
      : request_fn_(std::move(request_fn)), received_(0) {}

  static void stream_recv_cb(void *request, ucs_status_t status,
                             size_t received, void *user_data) {
    auto self = reinterpret_cast<stream_recv_awaitable *>(user_data);
    self->status_ = status;
    self->received_ = received;
    ::ucp_request_free(request);
    self->h_.resume();
  }

  bool await_ready() const noexcept { return false; }

  bool await_suspend(std::coroutine_handle<> h) {
    h_ = h;

    ucp_request_param_t stream_recv_param;
    stream_recv_param.op_attr_mask =
        UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
    stream_recv_param.cb.recv_stream = &stream_recv_cb;
    stream_recv_param.user_data = this;
    auto request = request_fn_(&stream_recv_param, &received_);

    status_ = UCS_PTR_STATUS(request);
    if (UCS_PTR_IS_ERR(request)) {
      return false;
    }
    return UCS_PTR_IS_PTR(request);
  }

  size_t await_resume() const {
    check_ucs_status(status_, "operation failed");
    return received_;
  }
};

/* Common awaitable class for tag-recv-like callbacks */
class tag_recv_awaitable {
public:
  using request_fn_t =
      std::function<ucs_status_ptr_t(ucp_request_param_t const *)>;

private:
  request_fn_t request_fn_;
  std::coroutine_handle<> h_;
  ucp_tag_recv_info_t recv_info_;
  ucs_status_t status_;

public:
  tag_recv_awaitable(request_fn_t &&request_fn)
      : request_fn_(std::move(request_fn)) {}

  static void tag_recv_cb(void *request, ucs_status_t status,
                          ucp_tag_recv_info_t const *tag_info,
                          void *user_data) {
    auto self = reinterpret_cast<tag_recv_awaitable *>(user_data);
    self->status_ = status;
    self->recv_info_.length = tag_info->length;
    self->recv_info_.sender_tag = tag_info->sender_tag;
    ::ucp_request_free(request);
    self->h_.resume();
  }

  bool await_ready() const noexcept { return false; }

  bool await_suspend(std::coroutine_handle<> h) {
    h_ = h;
    ucp_request_param_t tag_recv_param;
    tag_recv_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                                  UCP_OP_ATTR_FIELD_USER_DATA |
                                  UCP_OP_ATTR_FIELD_RECV_INFO;
    tag_recv_param.cb.recv = &tag_recv_cb;
    tag_recv_param.user_data = this;
    tag_recv_param.recv_info.tag_info = &recv_info_;

    auto request = request_fn_(&tag_recv_param);
    status_ = UCS_PTR_STATUS(request);
    if (UCS_PTR_IS_ERR(request)) {
      return false;
    }
    return UCS_PTR_IS_PTR(request);
  }

  std::pair<size_t, ucp_tag_t> await_resume() const {
    check_ucs_status(status_, "error in ucp_tag_recv_nbx");
    return std::make_pair(recv_info_.length, recv_info_.sender_tag);
  }
};

} // namespace ucxpp