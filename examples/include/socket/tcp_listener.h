#pragma once

#include "socket/event_loop.h"
#include <memory>

#include "ucxpp/detail/noncopyable.h"

namespace ucxpp {
namespace socket {

/**
 * @brief This class is used to listen for incoming TCP connections.
 *
 */
class tcp_listener : public noncopyable {
  std::shared_ptr<channel> channel_;

public:
  struct accept_awaitable {
    std::shared_ptr<channel> channel_;
    void *buffer_;
    int client_fd_;
    int do_io();

  public:
    accept_awaitable(std::shared_ptr<channel> channel);
    bool await_ready();
    void await_suspend(std::coroutine_handle<> h);
    std::shared_ptr<channel> await_resume();
  };
  tcp_listener(std::shared_ptr<event_loop> loop, std::string const &hostname,
               uint16_t port);
  accept_awaitable accept();
};

} // namespace socket
} // namespace ucxpp