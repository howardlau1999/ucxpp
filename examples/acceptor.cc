#include "acceptor.h"

#include "ep_transmission.h"
#include "socket/tcp_connection.h"

#include "ucxpp/address.h"
#include "ucxpp/endpoint.h"

namespace ucxpp {

acceptor::acceptor(std::shared_ptr<worker> worker,
                   std::shared_ptr<socket::tcp_listener> listener)
    : worker_(worker), listener_(listener), address_(worker->get_address()) {}

task<std::shared_ptr<endpoint>> acceptor::accept() {
  auto channel = co_await listener_->accept();
  socket::tcp_connection connection(channel);
  auto endpoint = co_await from_tcp_connection(connection, worker_);
  co_await send_address(address_, connection);
  co_return endpoint;
}

} // namespace ucxpp