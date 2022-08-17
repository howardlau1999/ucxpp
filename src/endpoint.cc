#include "ucxpp/endpoint.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <ucp/api/ucp.h>

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

std::shared_ptr<worker> endpoint::worker_ptr() { return worker_; }

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

send_awaitable endpoint::stream_send(void const *buffer, size_t length) {
  return send_awaitable([=, ep = this->shared_from_this()](auto param) {
    return ::ucp_stream_send_nbx(ep->ep_, buffer, length, param);
  });
}

stream_recv_awaitable endpoint::stream_recv(void *buffer, size_t length) {
  return stream_recv_awaitable(
      [=, ep = this->shared_from_this()](auto param, auto received) {
        return ::ucp_stream_recv_nbx(ep->ep_, buffer, length, received, param);
      });
}

send_awaitable endpoint::tag_send(void const *buffer, size_t length,
                                  ucp_tag_t tag) {
  return send_awaitable([=, ep = this->shared_from_this()](auto param) {
    return ::ucp_tag_send_nbx(ep->ep_, buffer, length, tag, param);
  });
}

tag_recv_awaitable endpoint::tag_recv(void *buffer, size_t length,
                                      ucp_tag_t tag, ucp_tag_t tag_mask) {
  return tag_recv_awaitable([=, ep = this->shared_from_this()](auto param) {
    return ::ucp_tag_recv_nbx(ep->worker_ptr()->handle(), buffer, length, tag,
                              tag_mask, param);
  });
}

send_awaitable endpoint::rma_put(void const *buffer, size_t length,
                                 uint64_t raddr, ucp_rkey_h rkey) {
  return send_awaitable([=, ep = this->shared_from_this()](auto param) {
    return ::ucp_put_nbx(ep->ep_, buffer, length, raddr, rkey, param);
  });
}

send_awaitable endpoint::rma_get(void *buffer, size_t length, uint64_t raddr,
                                 ucp_rkey_h rkey) {
  return send_awaitable([=, ep = this->shared_from_this()](auto param) {
    return ::ucp_get_nbx(ep->ep_, buffer, length, raddr, rkey, param);
  });
}

send_awaitable endpoint::flush() {
  return send_awaitable([ep = this->shared_from_this()](auto param) {
    return ::ucp_ep_flush_nbx(ep->ep_, param);
  });
}

send_awaitable endpoint::close() {
  return send_awaitable([ep = this->shared_from_this()](auto param) {
    return ::ucp_ep_close_nbx(ep->ep_, param);
  });
}

endpoint::~endpoint() {}

} // namespace ucxpp