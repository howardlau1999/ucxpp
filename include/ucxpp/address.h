#pragma once

#include <memory>
#include <vector>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

namespace ucxpp {

class worker;
class local_address {
  std::shared_ptr<worker> worker_;
  ucp_address_t *address_;
  size_t address_length_;
  friend class endpoint;

public:
  local_address(std::shared_ptr<worker> worker, ucp_address_t *address,
                size_t address_length);
  ~local_address();
};

class remote_address {
  std::vector<char> address_;

public:
  remote_address(std::vector<char> const &address);
  remote_address(std::vector<char> &&address);
  const ucp_address_t *get_address() const;
};

} // namespace ucxpp