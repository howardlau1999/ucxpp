#include "ucxpp/endpoint.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/address.h"
#include "ucxpp/awaitable.h"
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
  size_t address_length_read = 0;
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
  size_t address_read = 0;
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

std::shared_ptr<worker> endpoint::worker_ptr() const { return worker_; }

void endpoint::print() const { ::ucp_ep_print_info(ep_, stdout); }

ucp_ep_h endpoint::handle() const { return ep_; }

stream_send_awaitable endpoint::stream_send(void const *buffer,
                                            size_t length) const {
  return stream_send_awaitable(ep_, buffer, length);
}

stream_recv_awaitable endpoint::stream_recv(void *buffer, size_t length) const {
  return stream_recv_awaitable(ep_, buffer, length);
}

tag_send_awaitable endpoint::tag_send(void const *buffer, size_t length,
                                      ucp_tag_t tag) const {
  return tag_send_awaitable(ep_, buffer, length, tag);
}

tag_recv_awaitable endpoint::tag_recv(void *buffer, size_t length,
                                      ucp_tag_t tag, ucp_tag_t tag_mask) const {
  return tag_recv_awaitable(worker_->worker_, buffer, length, tag, tag_mask);
}

ep_flush_awaitable endpoint::flush() const { return ep_flush_awaitable(ep_); }

ep_close_awaitable endpoint::close() const { return ep_close_awaitable(ep_); }

endpoint::~endpoint() {}

} // namespace ucxpp