#pragma once

#include <cstdint>
#include <memory>

#include <ucp/api/ucp_def.h>

#include "ucxpp/awaitable.h"
#include "ucxpp/context.h"

#include "ucxpp/detail/noncopyable.h"

namespace ucxpp {

class endpoint;
class packed_memory_rkey : public noncopyable {
  void *buffer_;
  size_t length_;

public:
  packed_memory_rkey(void *buffer, size_t length);
  packed_memory_rkey(packed_memory_rkey &&other);
  void *get_buffer();
  void const *get_buffer() const;
  size_t get_length() const;
  ~packed_memory_rkey();
};

class local_memory_handle : public noncopyable {
  std::shared_ptr<context> ctx_;
  ucp_mem_h mem_;

public:
  local_memory_handle(std::shared_ptr<context> ctx, ucp_mem_h mem);
  local_memory_handle(local_memory_handle &&other);
  static std::pair<void *, local_memory_handle>
  allocate_mem(std::shared_ptr<context> ctx, size_t length);
  static local_memory_handle register_mem(std::shared_ptr<context> ctx,
                                          void *address, size_t length);
  ucp_mem_h handle() const;
  packed_memory_rkey pack_rkey() const;
  ~local_memory_handle();
};

class remote_memory_handle : public noncopyable {
  std::shared_ptr<endpoint> endpoint_;
  ucp_ep_h ep_;
  ucp_rkey_h rkey_;

public:
  remote_memory_handle(std::shared_ptr<endpoint> endpoint,
                       void const *packed_rkey_buffer);
  remote_memory_handle(remote_memory_handle &&other);
  rma_put_awaitable put(void const *buffer, size_t length,
                        uint64_t remote_addr) const;
  rma_get_awaitable get(void *buffer, size_t length,
                        uint64_t remote_addr) const;
  rma_put_awaitable write(void const *buffer, size_t length,
                          uint64_t remote_addr) const;
  rma_get_awaitable read(void *buffer, size_t length,
                         uint64_t remote_addr) const;

  std::shared_ptr<endpoint> endpoint_ptr() const;
  ucp_rkey_h handle() const;

  template <class T>
  rma_atomic_awaitable<T> atomic_fetch_add(uint64_t remote_addr, T const &delta,
                                           T &old_value) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_ADD, &delta, remote_addr,
                                   rkey_, &old_value);
  }

  template <class T>
  rma_atomic_awaitable<T> atomic_fetch_and(uint64_t remote_addr, T const &bits,
                                           T &old_value) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_AND, &bits, remote_addr,
                                   rkey_, &old_value);
  }

  template <class T>
  rma_atomic_awaitable<T> atomic_fetch_or(uint64_t remote_addr, T const &bits,
                                          T &old_value) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_OR, &bits, remote_addr,
                                   rkey_, &old_value);
  }

  template <class T>
  rma_atomic_awaitable<T> atomic_fetch_xor(uint64_t remote_addr, T const &bits,
                                           T &old_value) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_XOR, &bits, remote_addr,
                                   rkey_, &old_value);
  }

  template <class T>
  rma_atomic_awaitable<T> atomic_add(uint64_t remote_addr, T const &delta,
                                     T &old_value) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_ADD, &delta, remote_addr,
                                   rkey_, &old_value);
  }

  template <class T>
  rma_atomic_awaitable<T> atomic_and(uint64_t remote_addr,
                                     T const &bits) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_AND, &bits, remote_addr,
                                   rkey_);
  }

  template <class T>
  rma_atomic_awaitable<T> atomic_or(uint64_t remote_addr, T const &bits) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_OR, &bits, remote_addr,
                                   rkey_);
  }

  template <class T>
  rma_atomic_awaitable<T> atomic_xor(uint64_t remote_addr,
                                     T const &bits) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_XOR, &bits, remote_addr,
                                   rkey_);
  }

  template <class T>
  rma_atomic_awaitable<T> atomic_swap(uint64_t remote_addr, T const &new_value,
                                      T &old_value) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_SWAP, &new_value,
                                   remote_addr, rkey_, &old_value);
  }

  template <class T>
  rma_atomic_awaitable<T> atomic_compare_swap(uint64_t raddr, T const &expected,
                                              T &desired_and_old) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_CSWAP, &expected, raddr,
                                   rkey_, &desired_and_old);
  }

  ~remote_memory_handle();
};

} // namespace ucxpp