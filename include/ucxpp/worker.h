#pragma once

#include <memory>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/address.h"
#include "ucxpp/context.h"

namespace ucxpp {

class worker : public std::enable_shared_from_this<worker> {
  friend class local_address;
  friend class endpoint;
  ucp_worker_h worker_;

public:
  worker(std::shared_ptr<context> ctx);
  local_address get_address();
  bool progress();
  void wait();
  ~worker();
};

} // namespace ucxpp