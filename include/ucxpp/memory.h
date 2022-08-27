#pragma once

#include <cstdint>
#include <memory>

#include "ucxpp/awaitable.h"
#include "ucxpp/context.h"

#include "ucxpp/detail/noncopyable.h"

namespace ucxpp {

class endpoint;

/**
 * @brief A serializable UCX memory handle ready to send to peer.
 *
 */
class packed_memory_rkey : public noncopyable {
  friend class local_memory_handle;
  void *buffer_;
  size_t length_;
  packed_memory_rkey(void *buffer, size_t length);

public:
  packed_memory_rkey(packed_memory_rkey &&other);
  void *get_buffer();
  void const *get_buffer() const;
  size_t get_length() const;
  ~packed_memory_rkey();
};

/**
 * @brief Represents a registered local memory region.
 *
 */
class local_memory_handle : public noncopyable {
  std::shared_ptr<context> ctx_;
  ucp_mem_h mem_;

public:
  /**
   * @brief Construct a new local memory handle object
   *
   * @param ctx UCX context
   * @param mem UCX memory handle
   */
  local_memory_handle(std::shared_ptr<context> ctx, ucp_mem_h mem);

  /**
   * @brief Construct a new local memory handle object
   *
   * @param other Another local memory handle to move from
   */
  local_memory_handle(local_memory_handle &&other);

  /**
   * @brief Allocate and register a local memory region
   *
   * @param ctx UCX context
   * @param length Desired length of the memory region
   * @return std::pair<void *, local_memory_handle> A pair of pointer to the
   * allocated memory region and the local memory handle
   */
  static std::pair<void *, local_memory_handle>
  allocate_mem(std::shared_ptr<context> ctx, size_t length);
  static local_memory_handle register_mem(std::shared_ptr<context> ctx,
                                          void *address, size_t length);

  /**
   * @brief Get the native UCX memory handle
   *
   * @return ucp_mem_h
   */
  ucp_mem_h handle() const;

  /**
   * @brief Pack the information needed for remote access to the memory region.
   * It is intended to be sent to the remote peer.
   *
   * @return packed_memory_rkey The packed memory handle
   */
  packed_memory_rkey pack_rkey() const;

  /**
   * @brief Destroy the local memory handle object. The memory region will be
   * deregistered.
   *
   */
  ~local_memory_handle();
};

/**
 * @brief Represents a remote memory region. Note that this does not contain the
 * remote address. It should be kept by the user.
 *
 */
class remote_memory_handle : public noncopyable {
  std::shared_ptr<endpoint> endpoint_;
  ucp_ep_h ep_;
  ucp_rkey_h rkey_;

public:
  /**
   * @brief Construct a new remote memory handle object. All subsequent remote
   * memory access will happen on the given endpoint.
   *
   * @param endpoint UCX endpoint
   * @param packed_rkey_buffer Packed remote key buffer received from remote
   * peer
   */
  remote_memory_handle(std::shared_ptr<endpoint> endpoint,
                       void const *packed_rkey_buffer);

  /**
   * @brief Construct a new remote memory handle object
   *
   * @param other Another remote memory handle to move from
   */
  remote_memory_handle(remote_memory_handle &&other);

  /**
   * @brief Write to the remote memory region
   *
   * @param buffer Local buffer to write from
   * @param length Length of the buffer
   * @param remote_addr Remote address to write to
   * @return rma_put_awaitable A coroutine that returns upon completion
   */
  rma_put_awaitable put(void const *buffer, size_t length,
                        uint64_t remote_addr) const;

  /**
   * @brief Read from the remote memory region
   *
   * @param buffer Local buffer to read into
   * @param length Length of the buffer
   * @param remote_addr Remote address to read from
   * @return rma_get_awaitable A coroutine that returns upon completion
   */
  rma_get_awaitable get(void *buffer, size_t length,
                        uint64_t remote_addr) const;

  /**
   * \copydoc endpoint::put
   *
   */
  rma_put_awaitable write(void const *buffer, size_t length,
                          uint64_t remote_addr) const;

  /**
   * \copydoc endpoint::get
   *
   */
  rma_get_awaitable read(void *buffer, size_t length,
                         uint64_t remote_addr) const;

  /**
   * @brief Get the memory region's endpoint object
   *
   * @return std::shared_ptr<endpoint> The memory region's endpoint object
   */
  std::shared_ptr<endpoint> endpoint_ptr() const;

  /**
   * @brief Get the native UCX rkey handle
   *
   * @return ucp_rkey_h The native UCX rkey handle
   */
  ucp_rkey_h handle() const;

  /**
   * @brief Atomically fetch and add a value to the remote memory region
   *
   * @tparam T The type of the value to add, should be of 4 bytes or 8 bytes
   * long
   * @param remote_addr The remote address to add to
   * @param delta The value to add
   * @param old_value A reference to a variable to store the old value
   * @return rma_atomic_awaitable<T> A coroutine that returns upon completion.
   * The old value is placed in old_value
   */
  template <class T>
  rma_atomic_awaitable<T> atomic_fetch_add(uint64_t remote_addr, T const &delta,
                                           T &old_value) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_ADD, &delta, remote_addr,
                                   rkey_, &old_value);
  }

  /**
   * @brief Atomically fetch and AND a value to the remote memory region
   *
   * @tparam T The type of the value to AND, should be of 4 bytes or 8 bytes
   * long
   * @param remote_addr The remote address to AND to
   * @param delta The other operand of the AND operation
   * @param old_value A reference to a variable to store the old value
   * @return rma_atomic_awaitable<T> A coroutine that returns upon completion.
   * The old value is placed in old_value
   */
  template <class T>
  rma_atomic_awaitable<T> atomic_fetch_and(uint64_t remote_addr, T const &bits,
                                           T &old_value) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_AND, &bits, remote_addr,
                                   rkey_, &old_value);
  }

  /**
   * @brief Atomically fetch and OR a value to the remote memory region
   *
   * @tparam T The type of the value to OR, should be of 4 bytes or 8 bytes
   * long
   * @param remote_addr The remote address to OR to
   * @param delta The other operand of the OR operation
   * @param old_value A reference to a variable to store the old value
   * @return rma_atomic_awaitable<T> A coroutine that returns upon completion.
   * The old value is placed in old_value
   */
  template <class T>
  rma_atomic_awaitable<T> atomic_fetch_or(uint64_t remote_addr, T const &bits,
                                          T &old_value) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_OR, &bits, remote_addr,
                                   rkey_, &old_value);
  }

  /**
   * @brief Atomically fetch and XOR a value to the remote memory region
   *
   * @tparam T The type of the value to XOR, should be of 4 bytes or 8 bytes
   * long
   * @param remote_addr The remote address to XOR to
   * @param delta The other operand of the XOR operation
   * @param old_value A reference to a variable to store the old value
   * @return rma_atomic_awaitable<T> A coroutine that returns upon completion.
   * The old value is placed in old_value
   */
  template <class T>
  rma_atomic_awaitable<T> atomic_fetch_xor(uint64_t remote_addr, T const &bits,
                                           T &old_value) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_XOR, &bits, remote_addr,
                                   rkey_, &old_value);
  }

  /**
   * @brief Atomically add to a value in the remote memory region
   *
   * @tparam T The type of the value to add, should be of 4 bytes or 8 bytes
   * long
   * @param remote_addr The remote address to add to
   * @param delta The value to add
   * @return rma_atomic_awaitable<T> A coroutine that returns upon completion.
   */
  template <class T>
  rma_atomic_awaitable<T> atomic_add(uint64_t remote_addr,
                                     T const &delta) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_ADD, &delta, remote_addr,
                                   rkey_);
  }

  /**
   * @brief Atomically AND a value in the remote memory region
   *
   * @tparam T The type of the value to AND, should be of 4 bytes or 8 bytes
   * long
   * @param remote_addr The remote address to AND to
   * @param delta The other operand of the AND operation
   * @return rma_atomic_awaitable<T> A coroutine that returns upon completion.
   */
  template <class T>
  rma_atomic_awaitable<T> atomic_and(uint64_t remote_addr,
                                     T const &bits) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_AND, &bits, remote_addr,
                                   rkey_);
  }

  /**
   * @brief Atomically OR to a value in the remote memory region
   *
   * @tparam T The type of the value to OR, should be of 4 bytes or 8 bytes long
   * @param remote_addr The remote address to OR to
   * @param delta The other operand of the OR operation
   * @return rma_atomic_awaitable<T> A coroutine that returns upon completion.
   */
  template <class T>
  rma_atomic_awaitable<T> atomic_or(uint64_t remote_addr, T const &bits) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_OR, &bits, remote_addr,
                                   rkey_);
  }

  /**
   * @brief Atomically XOR a value to the remote memory region
   *
   * @tparam T The type of the value to XOR, should be of 4 bytes or 8 bytes
   * long
   * @param remote_addr The remote address to XOR to
   * @param delta The other operand of the XOR operation
   * @return rma_atomic_awaitable<T> A coroutine that returns upon completion.
   * The old value is placed in old_value
   */
  template <class T>
  rma_atomic_awaitable<T> atomic_xor(uint64_t remote_addr,
                                     T const &bits) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_XOR, &bits, remote_addr,
                                   rkey_);
  }

  /**
   * @brief Atomically swap a value in the remote memory region
   *
   * @tparam T The type of the value to swap, should be of 4 bytes or 8 bytes
   * long
   * @param remote_addr The remote address to swap
   * @param new_value The new value to swap in
   * @param old_value A reference to a variable to store the old value
   * @return rma_atomic_awaitable<T> A coroutine that returns upon completion.
   * The old value is placed in old_value
   */
  template <class T>
  rma_atomic_awaitable<T> atomic_swap(uint64_t remote_addr, T const &new_value,
                                      T &old_value) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_SWAP, &new_value,
                                   remote_addr, rkey_, &old_value);
  }

  /**
   * @brief Atomically compare and swap a value in the remote memory region
   *
   * @tparam T The type of the value to swap, should be of 4 bytes or 8 bytes
   * long
   * @param raddr The remote address to swap
   * @param expected The expected value to compare against
   * @param desired_and_old A reference to a variable to store the desired new
   * value and the old value swapped out
   * @return rma_atomic_awaitable<T> A coroutine that returns upon completion.
   * The old value is placed in desired_and_old
   */
  template <class T>
  rma_atomic_awaitable<T> atomic_compare_swap(uint64_t raddr, T const &expected,
                                              T &desired_and_old) const {
    return rma_atomic_awaitable<T>(ep_, UCP_ATOMIC_OP_CSWAP, &expected, raddr,
                                   rkey_, &desired_and_old);
  }

  /**
   * @brief Destroy the remote memory handle object and the associated rkey
   * handle
   *
   */
  ~remote_memory_handle();
};

} // namespace ucxpp