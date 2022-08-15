#pragma once

#include <coroutine>
#include <memory>
#include <ucs/type/status.h>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/address.h"
#include "ucxpp/error.h"
#include "ucxpp/socket/tcp_connection.h"
#include "ucxpp/task.h"
#include "ucxpp/worker.h"

#include "ucxpp/detail/noncopyable.h"

namespace ucxpp {

class endpoint : public noncopyable,
                 public std::enable_shared_from_this<endpoint> {
  friend class worker;
  std::shared_ptr<worker> worker_;
  ucp_ep_h ep_;

public:
  class tag_send_awaitable {
  public:
    ucs_status_t status_;
    std::shared_ptr<endpoint> endpoint_;
    void *buffer_;
    size_t length_;
    ucp_tag_t tag_;
    std::coroutine_handle<> h_;
    tag_send_awaitable(std::shared_ptr<endpoint> endpoint, void *buffer,
                       size_t length, ucp_tag_t tag)
        : endpoint_(endpoint), buffer_(buffer), length_(length), tag_(tag) {}
    static void send_cb(void *request, ucs_status_t status, void *user_data) {
      auto self = reinterpret_cast<tag_send_awaitable *>(user_data);
      self->status_ = status;
      ::ucp_request_free(request);
      self->h_.resume();
    }
    bool await_ready() noexcept { return false; }
    bool await_suspend(std::coroutine_handle<> h) {
      h_ = h;
      ucp_request_param_t send_param;
      send_param.op_attr_mask =
          UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
      send_param.cb.send = &send_cb;
      send_param.user_data = this;
      auto request = ::ucp_tag_send_nbx(endpoint_->ep_, buffer_, length_, tag_,
                                        &send_param);
      if (UCS_PTR_IS_ERR(request)) {
        status_ = UCS_PTR_STATUS(request);
        return false;
      }
      return UCS_PTR_IS_PTR(request);
    }
    void await_resume() {
      check_ucs_status(status_, "error in ucp_tag_send_nb");
    }
  };

  class tag_recv_awaitable {
  public:
    ucs_status_t status_;
    std::shared_ptr<endpoint> endpoint_;
    void *buffer_;
    size_t length_;
    ucp_tag_t tag_;
    ucp_tag_t tag_mask_;
    std::coroutine_handle<> h_;
    tag_recv_awaitable(std::shared_ptr<endpoint> endpoint, void *buffer,
                       size_t length, ucp_tag_t tag, ucp_tag_t tag_mask)
        : endpoint_(endpoint), buffer_(buffer), length_(length), tag_(tag),
          tag_mask_(tag_mask) {}
    static void recv_cb(void *request, ucs_status_t status,
                        ucp_tag_recv_info_t const *tag_info, void *user_data) {
      auto self = reinterpret_cast<tag_recv_awaitable *>(user_data);
      self->status_ = status;
      ::ucp_request_free(request);
      self->h_.resume();
    }
    bool await_ready() noexcept { return false; }
    bool await_suspend(std::coroutine_handle<> h) {
      h_ = h;
      ucp_request_param_t recv_param;
      recv_param.op_attr_mask =
          UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
      recv_param.cb.recv = &recv_cb;
      recv_param.user_data = this;
      auto request = ::ucp_tag_recv_nbx(endpoint_->worker_->worker_, buffer_,
                                        length_, tag_, tag_mask_, &recv_param);
      if (UCS_PTR_IS_ERR(request)) {
        status_ = UCS_PTR_STATUS(request);
        return false;
      }
      return UCS_PTR_IS_PTR(request);
    }
    void await_resume() {
      check_ucs_status(status_, "error in ucp_tag_recv_nb");
    }
  };
  endpoint(std::shared_ptr<worker> worker, remote_address const &peer);
  static task<std::shared_ptr<endpoint>>
  from_tcp_connection(socket::tcp_connection &conncetion,
                      std::shared_ptr<worker> worker);
  tag_send_awaitable tag_send(void const *buffer, size_t length, ucp_tag_t tag);
  ~endpoint();
};

} // namespace ucxpp