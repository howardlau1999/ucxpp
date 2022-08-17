#pragma once

#include <coroutine>
#include <cstdint>
#include <memory>
#include <ucs/type/status.h>
#include <vector>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/address.h"
#include "ucxpp/awaitable.h"
#include "ucxpp/error.h"
#include "ucxpp/socket/tcp_connection.h"
#include "ucxpp/task.h"
#include "ucxpp/worker.h"

#include "ucxpp/detail/noncopyable.h"

namespace ucxpp {

class endpoint : public noncopyable,
                 public std::enable_shared_from_this<endpoint> {
  friend class worker;
  friend class local_memory_handle;
  friend class remote_memory_handle;
  std::shared_ptr<worker> worker_;
  ucp_ep_h ep_;

public:
  endpoint(std::shared_ptr<worker> worker, remote_address const &peer);
  std::shared_ptr<worker> worker_ptr();
  void print();
  static task<std::shared_ptr<endpoint>>
  from_tcp_connection(socket::tcp_connection &conncetion,
                      std::shared_ptr<worker> worker);

  send_awaitable stream_send(void const *buffer, size_t length);
  stream_recv_awaitable stream_recv(void *buffer, size_t length);
  send_awaitable tag_send(void const *buffer, size_t length, ucp_tag_t tag);
  tag_recv_awaitable tag_recv(void *buffer, size_t length, ucp_tag_t tag,
                              ucp_tag_t tag_mask = 0xFFFFFFFFFFFFFFFF);
  send_awaitable rma_put(void const *buffer, size_t length, uint64_t raddr,
                         ucp_rkey_h rkey);
  send_awaitable rma_get(void *buffer, size_t length, uint64_t raddr,
                         ucp_rkey_h rkey);
  send_awaitable flush();
  send_awaitable close();

  ~endpoint();
};

} // namespace ucxpp

/** \example helloworld.cc
 * This is an example of how to use the endpoint class.
 */