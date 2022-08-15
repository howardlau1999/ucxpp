#include "ucxpp/endpoint.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <ucp/api/ucp.h>

#include "ucxpp/address.h"
#include "ucxpp/error.h"

#include "ucxpp/detail/debug.h"
#include "ucxpp/detail/serdes.h"

namespace ucxpp {

endpoint::endpoint(std::shared_ptr<worker> worker, remote_address const &peer)
    : worker_(worker) {
  ucp_ep_params_t ep_params;
  ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
  ep_params.address = peer.get_address();
  check_ucs_status(::ucp_ep_create(worker_->worker_, &ep_params, &ep_),
                   "failed to create ep");
}

task<std::shared_ptr<endpoint>>
endpoint::from_tcp_connection(socket::tcp_connection &conncetion,
                              std::shared_ptr<worker> worker) {
  int address_length_read = 0;
  char address_length_buffer[sizeof(size_t)];
  while (address_length_read < sizeof(size_t)) {
    int n =
        co_await conncetion.recv(address_length_buffer + address_length_read,
                                 sizeof(size_t) - address_length_read);
    if (n == 0) {
      throw std::runtime_error("failed to read address length");
    }
    address_length_read += n;
  }
  size_t address_length;
  char *p = address_length_buffer;
  detail::deserialize(p, address_length);
  std::vector<char> address_buffer(address_length);
  int address_read = 0;
  while (address_read < address_length) {
    int n = co_await conncetion.recv(&address_buffer[address_read],
                                     address_length - address_read);
    if (n == 0) {
      throw std::runtime_error("failed to read address");
    }
    address_read += n;
  }
  auto remote_addr = remote_address(std::move(address_buffer));
  co_return std::make_shared<endpoint>(worker, remote_addr);
}

void endpoint::print() { ::ucp_ep_print_info(ep_, stdout); }

endpoint::stream_send_awaitable endpoint::stream_send(void const *buffer,
                                                      size_t length) {
  return stream_send_awaitable(this->shared_from_this(), buffer, length);
}

endpoint::stream_recv_awaitable endpoint::stream_recv(void *buffer,
                                                      size_t length) {
  return stream_recv_awaitable(this->shared_from_this(), buffer, length);
}

endpoint::tag_send_awaitable endpoint::tag_send(void const *buffer,
                                                size_t length, ucp_tag_t tag) {
  return tag_send_awaitable(this->shared_from_this(), buffer, length, tag);
}

endpoint::tag_recv_awaitable endpoint::tag_recv(void *buffer, size_t length,
                                                ucp_tag_t tag,
                                                ucp_tag_t tag_mask) {
  return tag_recv_awaitable(this->shared_from_this(), buffer, length, tag,
                            tag_mask);
}

endpoint::stream_send_awaitable::stream_send_awaitable(
    std::shared_ptr<endpoint> endpoint, void const *buffer, size_t length)
    : endpoint_(endpoint), buffer_(buffer), length_(length) {}

void endpoint::stream_send_awaitable::send_cb(void *request,
                                              ucs_status_t status,
                                              void *user_data) {}

bool endpoint::stream_send_awaitable::await_ready() noexcept { return false; }

bool endpoint::stream_send_awaitable::await_suspend(std::coroutine_handle<> h) {
  h_ = h;
  ucp_request_param_t send_param;
  send_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
  send_param.cb.send = &send_cb;
  send_param.user_data = this;
  auto request =
      ::ucp_stream_send_nbx(endpoint_->ep_, buffer_, length_, &send_param);
  if (UCS_PTR_IS_ERR(request)) {
    status_ = UCS_PTR_STATUS(request);
    return false;
  }
  return UCS_PTR_IS_PTR(request);
}

void endpoint::stream_send_awaitable::await_resume() {
  check_ucs_status(status_, "error in ucp_stream_send_nb");
}

endpoint::stream_recv_awaitable::stream_recv_awaitable(
    std::shared_ptr<endpoint> endpoint, void *buffer, size_t length)
    : endpoint_(endpoint), buffer_(buffer), length_(length), received_(0) {}

void endpoint::stream_recv_awaitable::recv_cb(void *request,
                                              ucs_status_t status,
                                              size_t received,
                                              void *user_data) {
  auto self = reinterpret_cast<stream_recv_awaitable *>(user_data);
  self->status_ = status;
  self->received_ = received;
  ::ucp_request_free(request);
  self->h_.resume();
}

bool endpoint::stream_recv_awaitable::await_ready() noexcept { return false; }

bool endpoint::stream_recv_awaitable::await_suspend(std::coroutine_handle<> h) {
  h_ = h;
  ucp_request_param_t recv_param;
  recv_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
  recv_param.cb.recv_stream = &recv_cb;
  recv_param.user_data = this;
  auto request = ::ucp_stream_recv_nbx(endpoint_->ep_, buffer_, length_,
                                       &received_, &recv_param);
  if (UCS_PTR_IS_ERR(request)) {
    status_ = UCS_PTR_STATUS(request);
    return false;
  }
  return UCS_PTR_IS_PTR(request);
}

size_t endpoint::stream_recv_awaitable::await_resume() {
  check_ucs_status(status_, "error in ucp_stream_recv_nbx");
  return received_;
}

endpoint::tag_send_awaitable::tag_send_awaitable(
    std::shared_ptr<endpoint> endpoint, void const *buffer, size_t length,
    ucp_tag_t tag)
    : endpoint_(endpoint), buffer_(buffer), length_(length), tag_(tag) {}

void endpoint::tag_send_awaitable::send_cb(void *request, ucs_status_t status,
                                           void *user_data) {
  auto self = reinterpret_cast<tag_send_awaitable *>(user_data);
  self->status_ = status;
  ::ucp_request_free(request);
  self->h_.resume();
}

bool endpoint::tag_send_awaitable::await_ready() noexcept { return false; }

bool endpoint::tag_send_awaitable::await_suspend(std::coroutine_handle<> h) {
  h_ = h;
  ucp_request_param_t send_param;
  send_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
  send_param.cb.send = &send_cb;
  send_param.user_data = this;
  auto request =
      ::ucp_tag_send_nbx(endpoint_->ep_, buffer_, length_, tag_, &send_param);
  if (UCS_PTR_IS_ERR(request)) {
    status_ = UCS_PTR_STATUS(request);
    return false;
  }
  return UCS_PTR_IS_PTR(request);
}

void endpoint::tag_send_awaitable::await_resume() {
  check_ucs_status(status_, "error in ucp_tag_send_nbx");
}

endpoint::tag_recv_awaitable::tag_recv_awaitable(
    std::shared_ptr<endpoint> endpoint, void *buffer, size_t length,
    ucp_tag_t tag, ucp_tag_t tag_mask)
    : endpoint_(endpoint), buffer_(buffer), length_(length), tag_(tag),
      tag_mask_(tag_mask) {}

void endpoint::tag_recv_awaitable::recv_cb(void *request, ucs_status_t status,
                                           ucp_tag_recv_info_t const *tag_info,
                                           void *user_data) {
  auto self = reinterpret_cast<tag_recv_awaitable *>(user_data);
  self->status_ = status;
  self->recv_info_.length = tag_info->length;
  self->recv_info_.sender_tag = tag_info->sender_tag;
  ::ucp_request_free(request);
  self->h_.resume();
}

bool endpoint::tag_recv_awaitable::await_ready() noexcept { return false; }

bool endpoint::tag_recv_awaitable::await_suspend(std::coroutine_handle<> h) {
  h_ = h;
  ucp_request_param_t recv_param;
  recv_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                            UCP_OP_ATTR_FIELD_USER_DATA |
                            UCP_OP_ATTR_FIELD_RECV_INFO;
  recv_param.cb.recv = &recv_cb;
  recv_param.user_data = this;
  recv_param.recv_info.tag_info = &recv_info_;
  auto request = ::ucp_tag_recv_nbx(endpoint_->worker_->worker_, buffer_,
                                    length_, tag_, tag_mask_, &recv_param);
  if (UCS_PTR_IS_ERR(request)) {
    status_ = UCS_PTR_STATUS(request);
    return false;
  }
  return UCS_PTR_IS_PTR(request);
}

std::pair<size_t, ucp_tag_t> endpoint::tag_recv_awaitable::await_resume() {
  check_ucs_status(status_, "error in ucp_tag_recv_nbx");
  return std::make_pair(recv_info_.length, recv_info_.sender_tag);
}

endpoint::~endpoint() {
  ::ucp_ep_close_nb(ep_, ucp_ep_close_mode::UCP_EP_CLOSE_MODE_FORCE);
}

} // namespace ucxpp