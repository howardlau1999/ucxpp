#pragma once

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <functional>
#include <ucs/type/status.h>

#include <ucp/api/ucp.h>

#include "ucxpp/error.h"
struct request_context {
  void *awaitable;
};

namespace ucxpp {

/* Common awaitable class for send-like callbacks */
class send_awaitable {
public:
  using request_fn_t =
      std::function<ucs_status_ptr_t(ucp_request_param_t const *)>;

private:
  request_fn_t request_fn_;
  std::atomic<std::coroutine_handle<>> h_;
  ucs_status_t status_;
  std::atomic<bool> done_;

public:
  send_awaitable(request_fn_t &&request_fn)
      : request_fn_(std::move(request_fn)) {}

  static void send_cb(void *request, ucs_status_t status, void *user_data) {
    auto self = static_cast<send_awaitable *>(user_data);
    self->status_ = status;
    ::ucp_request_free(request);
    self->done_.store(true);
    if (self->h_.load()) {
      self->h_.load().resume();
    }
  }

  bool await_ready() noexcept {
    ucp_request_param_t send_param;
    send_param.op_attr_mask =
        UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
    send_param.cb.send = &send_cb;
    send_param.user_data = this;
    auto request = request_fn_(&send_param);

    status_ = UCS_PTR_STATUS(request);
    if (UCS_PTR_IS_ERR(status_)) {
      return false;
    }

    return status_ == UCS_OK;
  }

  bool await_suspend(std::coroutine_handle<> h) {
    if (done_) {
      return false;
    }
    h_ = h;
    return true;
  }

  void await_resume() const { check_ucs_status(status_, "operation failed"); }
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