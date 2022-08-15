#include "ucxpp/acceptor.h"

#include "ucxpp/address.h"
#include "ucxpp/endpoint.h"
#include "ucxpp/socket/tcp_connection.h"

namespace ucxpp {

acceptor::acceptor(std::shared_ptr<worker> worker,
                   std::shared_ptr<socket::tcp_listener> listener)
    : worker_(worker), listener_(listener), address_(worker->get_address()) {}

task<std::shared_ptr<endpoint>> acceptor::accept() {
  auto channel = co_await listener_->accept();
  socket::tcp_connection connection(channel);
  auto endpoint = co_await endpoint::from_tcp_connection(connection, worker_);
  co_await address_.send_to(connection);
  co_return endpoint;
}

} // namespace ucxpp