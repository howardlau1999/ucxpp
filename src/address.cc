#include "ucxpp/address.h"

#include <ucp/api/ucp.h>

#include "ucxpp/worker.h"

namespace ucxpp {

local_address::local_address(std::shared_ptr<worker> worker,
                             ucp_address_t *address, size_t address_length)
    : worker_(worker), address_(address), address_length_(address_length) {}

local_address::~local_address() {
  if (address_ == nullptr) [[unlikely]] {
    return;
  }
  ::ucp_worker_release_address(worker_->worker_, address_);
}

remote_address::remote_address(std::vector<char> const &address)
    : address_(address) {}
remote_address::remote_address(std::vector<char> &&address)
    : address_(std::move(address)) {}
const ucp_address_t *remote_address::get_address() const {
  return reinterpret_cast<const ucp_address_t *>(address_.data());
}

} // namespace ucxpp