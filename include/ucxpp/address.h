#pragma once

#include <memory>
#include <vector>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/task.h"

#include "ucxpp/detail/noncopyable.h"
namespace ucxpp {

class worker;

/**
 * @brief Represents a local UCX address.
 *
 */
class local_address : public noncopyable {
  std::shared_ptr<worker> worker_;
  ucp_address_t *address_;
  size_t address_length_;
  friend class endpoint;

public:
  /**
   * @brief Construct a new local address object
   *
   * @param worker UCX worker
   * @param address UCP address
   * @param address_length UCP address length
   */
  local_address(std::shared_ptr<worker> worker, ucp_address_t *address,
                size_t address_length);

  /**
   * @brief Construct a new local address object
   *
   * @param other Another local address object to move from
   */
  local_address(local_address &&other);

  /**
   * @brief Move assignment operator
   *
   * @param other Another local address object to move from
   * @return local_address& This object
   */
  local_address &operator=(local_address &&other);

  /**
   * @brief Serialize the address to a buffer ready to be sent to a remote peer
   *
   * @return std::vector<char> The serialized address
   */
  std::vector<char> serialize() const;

  /**
   * @brief Get the UCP address
   *
   * @return const ucp_address_t* The UCP address
   */
  const ucp_address_t *get_address() const;

  /**
   * @brief Get the length of the address
   *
   * @return size_t The address length
   */
  size_t get_length() const;

  /**
   * @brief Destroy the local address object and release the buffer
   *
   */
  ~local_address();
};

/**
 * @brief Represents a remote UCX address.
 *
 */
class remote_address {
  std::vector<char> address_;

public:
  /**
   * @brief Construct a new remote address object
   *
   * @param address The received address buffer
   */
  remote_address(std::vector<char> const &address);

  /**
   * @brief Construct a new remote address object
   *
   * @param address Anothr remote address object to move from
   */
  remote_address(std::vector<char> &&address);

  /**
   * @brief Get the UCP address
   *
   * @return const ucp_address_t* The UCP address
   */
  const ucp_address_t *get_address() const;

  /**
   * @brief Get the length of the address
   *
   * @return size_t The length of the address
   */
  size_t get_length() const;
};

} // namespace ucxpp