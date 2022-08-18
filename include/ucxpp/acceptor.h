#pragma once

#include <memory>

#include "ucxpp/address.h"
#include "ucxpp/endpoint.h"
#include "ucxpp/socket/tcp_connection.h"
#include "ucxpp/socket/tcp_listener.h"
#include "ucxpp/task.h"

namespace ucxpp {

class acceptor {
  std::shared_ptr<worker> worker_;
  std::shared_ptr<socket::tcp_listener> listener_;
  local_address address_;

public:
  acceptor(std::shared_ptr<worker> worker,
           std::shared_ptr<socket::tcp_listener> listener);
  acceptor(acceptor &&) = default;
  acceptor &operator=(acceptor &&) = default;
  task<std::shared_ptr<endpoint>> accept();
};

} // namespace ucxpp