#pragma once

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <ucp/api/ucp.h>

namespace ucxpp {

class context : public std::enable_shared_from_this<context> {
  friend class worker;
  friend class local_memory_handle;
  ucp_context_h context_;

public:
  class builder {
    uint64_t features_;
    bool print_config_;

  public:
    builder();
    std::shared_ptr<context> build();
    builder enable_print_config();
    builder &enable_wakeup();
    builder &enable_tag();
    builder &enable_stream();
    builder &enable_am();
    builder &enable_rma();
    builder &enable_amo32();
    builder &enable_amo64();
  };
  context(uint64_t features, bool print_config);
  ucp_context_h handle();
  ~context();
};

} // namespace ucxpp