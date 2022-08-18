#pragma once

#include <coroutine>
#include <cstddef>
#include <functional>

#include <ucp/api/ucp.h>

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
  std::coroutine_handle<> h_;
  ucs_status_t status_;
  request_fn_t request_fn_;

public:
  send_awaitable(request_fn_t &&request_fn);
  static void send_cb(void *request, ucs_status_t status, void *user_data);
  bool await_ready() const noexcept;
  bool await_suspend(std::coroutine_handle<> h);
  void await_resume() const;
};

/* Common awaitable class for stream-recv-like callbacks */
class stream_recv_awaitable {
public:
  using request_fn_t = std::function<ucs_status_ptr_t(
      ucp_request_param_t const *, size_t *received)>;

private:
  std::coroutine_handle<> h_;
  ucs_status_t status_;
  request_fn_t request_fn_;
  size_t received_;

public:
  stream_recv_awaitable(request_fn_t &&request_fn);
  static void stream_recv_cb(void *request, ucs_status_t status,
                             size_t received, void *user_data);
  bool await_ready() const noexcept;
  bool await_suspend(std::coroutine_handle<> h);
  size_t await_resume() const;
};

/* Common awaitable class for tag-recv-like callbacks */
class tag_recv_awaitable {
public:
  using request_fn_t =
      std::function<ucs_status_ptr_t(ucp_request_param_t const *)>;

private:
  std::coroutine_handle<> h_;
  ucs_status_t status_;
  request_fn_t request_fn_;
  ucp_tag_recv_info_t recv_info_;

public:
  tag_recv_awaitable(request_fn_t &&request_fn);
  static void tag_recv_cb(void *request, ucs_status_t status,
                          ucp_tag_recv_info_t const *tag_info, void *user_data);
  bool await_ready() const noexcept;
  bool await_suspend(std::coroutine_handle<> h);
  std::pair<size_t, ucp_tag_t> await_resume() const;
};

} // namespace ucxpp