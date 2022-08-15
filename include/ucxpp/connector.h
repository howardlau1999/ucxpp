#pragma once

#include <memory>

#include "ucxpp/address.h"
#include "ucxpp/endpoint.h"
#include "ucxpp/socket/tcp_connection.h"
#include "ucxpp/worker.h"

namespace ucxpp {

class connector {
  std::shared_ptr<worker> worker_;
  std::shared_ptr<socket::event_loop> loop_;
  std::string hostname_;
  uint16_t port_;
  local_address address_;

public:
  connector(std::shared_ptr<worker> worker,
            std::shared_ptr<socket::event_loop> loop,
            std::string const &hostname, uint16_t port);
  task<std::shared_ptr<endpoint>> connect();
};

} // namespace ucxpp