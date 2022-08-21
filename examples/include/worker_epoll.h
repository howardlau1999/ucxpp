#pragma once

#include "socket/channel.h"
#include "socket/event_loop.h"
#include <memory>

#include <ucxpp/worker.h>

namespace ucxpp {

void register_loop(std::shared_ptr<worker> worker,
                   std::shared_ptr<socket::event_loop> loop);

} // namespace ucxpp