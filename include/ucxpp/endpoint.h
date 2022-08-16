#pragma once

#include <coroutine>
#include <cstdint>
#include <memory>
#include <ucs/type/status.h>
#include <vector>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/address.h"
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
  class stream_send_awaitable {
    ucs_status_t status_;
    std::shared_ptr<endpoint> endpoint_;
    void const *buffer_;
    size_t length_;
    std::coroutine_handle<> h_;

  public:
    stream_send_awaitable(std::shared_ptr<endpoint> endpoint,
                          void const *buffer, size_t length);
    static void send_cb(void *request, ucs_status_t status, void *user_data);
    bool await_ready() noexcept;
    bool await_suspend(std::coroutine_handle<> h);
    void await_resume();
  };

  class stream_recv_awaitable {
    ucs_status_t status_;
    std::shared_ptr<endpoint> endpoint_;
    void *buffer_;
    size_t length_;
    size_t received_;
    std::coroutine_handle<> h_;

  public:
    stream_recv_awaitable(std::shared_ptr<endpoint> endpoint, void *buffer,
                          size_t length);
    static void recv_cb(void *request, ucs_status_t status, size_t length,
                        void *user_data);
    bool await_ready() noexcept;
    bool await_suspend(std::coroutine_handle<> h);
    size_t await_resume();
  };

  class tag_send_awaitable {
    ucs_status_t status_;
    std::shared_ptr<endpoint> endpoint_;
    void const *buffer_;
    size_t length_;
    ucp_tag_t tag_;
    std::coroutine_handle<> h_;

  public:
    tag_send_awaitable(std::shared_ptr<endpoint> endpoint, void const *buffer,
                       size_t length, ucp_tag_t tag);
    static void send_cb(void *request, ucs_status_t status, void *user_data);
    bool await_ready() noexcept;
    bool await_suspend(std::coroutine_handle<> h);
    void await_resume();
  };

  class tag_recv_awaitable {
    ucs_status_t status_;
    std::shared_ptr<endpoint> endpoint_;
    void *buffer_;
    size_t length_;
    ucp_tag_recv_info recv_info_;
    ucp_tag_t tag_;
    ucp_tag_t tag_mask_;
    std::coroutine_handle<> h_;

  public:
    tag_recv_awaitable(std::shared_ptr<endpoint> endpoint, void *buffer,
                       size_t length, ucp_tag_t tag, ucp_tag_t tag_mask);
    static void recv_cb(void *request, ucs_status_t status,
                        ucp_tag_recv_info_t const *tag_info, void *user_data);
    bool await_ready() noexcept;
    bool await_suspend(std::coroutine_handle<> h);
    std::pair<size_t, ucp_tag_t> await_resume();
  };

  class rma_put_awaitable {
    ucs_status_t status_;
    std::shared_ptr<endpoint> endpoint_;
    void const *buffer_;
    size_t length_;
    uint64_t raddr_;
    ucp_rkey_h rkey_;
    std::coroutine_handle<> h_;

  public:
    rma_put_awaitable(std::shared_ptr<endpoint> endpoint, void const *buffer,
                      size_t length, uint64_t raddr, ucp_rkey_h rkey);
    static void send_cb(void *request, ucs_status_t status, void *user_data);
    bool await_ready() noexcept;
    bool await_suspend(std::coroutine_handle<> h);
    void await_resume();
  };

  class rma_get_awaitable {
    ucs_status_t status_;
    std::shared_ptr<endpoint> endpoint_;
    void *buffer_;
    size_t length_;
    size_t received_;
    uint64_t raddr_;
    ucp_rkey_h rkey_;
    std::coroutine_handle<> h_;

  public:
    rma_get_awaitable(std::shared_ptr<endpoint> endpoint, void *buffer,
                      size_t length, uint64_t raddr, ucp_rkey_h rkey);
    static void send_cb(void *request, ucs_status_t status, void *user_data);
    bool await_ready() noexcept;
    bool await_suspend(std::coroutine_handle<> h);
    void await_resume();
  };

  endpoint(std::shared_ptr<worker> worker, remote_address const &peer);
  void print();
  static task<std::shared_ptr<endpoint>>
  from_tcp_connection(socket::tcp_connection &conncetion,
                      std::shared_ptr<worker> worker);

  stream_send_awaitable stream_send(void const *buffer, size_t length);
  stream_recv_awaitable stream_recv(void *buffer, size_t length);
  tag_send_awaitable tag_send(void const *buffer, size_t length, ucp_tag_t tag);
  tag_recv_awaitable tag_recv(void *buffer, size_t length, ucp_tag_t tag,
                              ucp_tag_t tag_mask = 0xFFFFFFFF);
  rma_put_awaitable rma_put(void const *buffer, size_t length, uint64_t raddr,
                            ucp_rkey_h rkey);
  rma_get_awaitable rma_get(void *buffer, size_t length, uint64_t raddr,
                            ucp_rkey_h rkey);

  ~endpoint();
};

} // namespace ucxpp

/** \example helloworld.cc
 * This is an example of how to use the endpoint class.
 */