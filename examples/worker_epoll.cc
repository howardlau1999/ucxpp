#include "worker_epoll.h"

#include "socket/channel.h"

namespace ucxpp {

void register_loop(std::shared_ptr<worker> worker,
                   std::shared_ptr<socket::event_loop> loop) {
  auto event_channel =
      std::make_shared<socket::channel>(worker->event_fd(), loop);
  event_channel->set_event_loop(loop);
  event_channel->set_readable_callback([worker, event_channel]() {
    do {
      while (worker->progress()) {
      }
    } while (!worker->arm());
    if (worker.use_count() > 2) {
      event_channel->wait_readable();
    } else {
      event_channel->set_event_loop(nullptr);
      event_channel->set_readable_callback([]() {});
    }
  });
  event_channel->wait_readable();
}

} // namespace ucxpp