#pragma once

#include <functional>
#include <memory>
#include <ucs/type/status.h>
#include <unordered_map>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/address.h"
#include "ucxpp/awaitable.h"
#include "ucxpp/context.h"
#include "ucxpp/socket/channel.h"

namespace ucxpp {

class worker : public std::enable_shared_from_this<worker> {
  friend class local_address;
  friend class endpoint;
  ucp_worker_h worker_;
  std::shared_ptr<context> ctx_;
  std::unordered_map<ucs_status_ptr_t, std::function<void(ucs_status_t)>>
      pending_;
  std::mutex pending_mutex_;
  std::shared_ptr<socket::channel> event_channel_;

public:
  worker(std::shared_ptr<context> ctx);
  worker(std::shared_ptr<context> ctx,
         std::shared_ptr<socket::event_loop> loop);
  std::shared_ptr<socket::channel> event_channel() const;
  void register_loop(std::shared_ptr<socket::event_loop> loop);
  std::shared_ptr<context> context_ptr();
  local_address get_address();
  ucp_worker_h handle();
  bool progress();
  void wait();
  worker_flush_awaitable flush();
  void check_pending();
  void add_pending(ucs_status_ptr_t status,
                   std::function<void(ucs_status_t)> &&callback);
  ~worker();
};

} // namespace ucxpp