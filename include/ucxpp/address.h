#pragma once

#include <memory>
#include <vector>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/socket/tcp_connection.h"
#include "ucxpp/task.h"

#include "ucxpp/detail/noncopyable.h"
namespace ucxpp {

class worker;
class local_address : public noncopyable {
  std::shared_ptr<worker> worker_;
  ucp_address_t *address_;
  size_t address_length_;
  friend class endpoint;

public:
  local_address(std::shared_ptr<worker> worker, ucp_address_t *address,
                size_t address_length);
  local_address(local_address &&other);
  local_address &operator=(local_address &&other);
  std::vector<char> serialize() const;
  const ucp_address_t *get_address() const;
  size_t get_length() const;
  task<void> send_to(socket::tcp_connection &connection);
  ~local_address();
};

class remote_address {
  std::vector<char> address_;

public:
  remote_address(std::vector<char> const &address);
  remote_address(std::vector<char> &&address);
  const ucp_address_t *get_address() const;
  size_t get_length() const;
};

} // namespace ucxpp