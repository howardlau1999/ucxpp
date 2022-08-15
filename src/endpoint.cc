#include "ucxpp/endpoint.h"

#include <memory>
#include <vector>

#include <ucp/api/ucp.h>

#include "ucxpp/address.h"
#include "ucxpp/error.h"

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
  detail::deserialize(reinterpret_cast<char *&>(address_length_buffer),
                      address_length);
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

endpoint::~endpoint() {
  ::ucp_ep_close_nb(ep_, ucp_ep_close_mode::UCP_EP_CLOSE_MODE_FORCE);
}

} // namespace ucxpp