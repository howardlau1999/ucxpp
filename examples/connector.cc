#include "connector.h"

#include "ep_transmission.h"

#include "ucxpp/address.h"

namespace ucxpp {

connector::connector(std::shared_ptr<worker> worker,
                     std::shared_ptr<socket::event_loop> loop,
                     std::string const &hostname, uint16_t port)
    : worker_(worker), loop_(loop), hostname_(hostname), port_(port),
      address_(worker->get_address()) {}

task<std::shared_ptr<endpoint>> connector::connect() {
  auto connection =
      co_await socket::tcp_connection::connect(loop_, hostname_, port_);
  co_await send_address(address_, *connection);
  auto endpoint = co_await from_tcp_connection(*connection, worker_);
  co_return endpoint;
}

} // namespace ucxpp