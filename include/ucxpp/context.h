#pragma once
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <ucp/api/ucp.h>

namespace ucxpp {

class context {
  friend class worker;
  ucp_context_h context_;

public:
  context(bool print_config = false);
  ~context();
};

} // namespace ucxpp