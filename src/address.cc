#include "ucxpp/address.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include <ucp/api/ucp.h>

#include "ucxpp/task.h"
#include "ucxpp/worker.h"

#include "ucxpp/detail/debug.h"
#include "ucxpp/detail/serdes.h"

namespace ucxpp {

local_address::local_address(std::shared_ptr<worker const> worker,
                             ucp_address_t *address, size_t address_length)
    : worker_(worker), address_(address), address_length_(address_length) {}

local_address::local_address(local_address &&other)
    : worker_(std::move(other.worker_)),
      address_(std::exchange(other.address_, nullptr)),
      address_length_(other.address_length_) {}

local_address &local_address::operator=(local_address &&other) {
  worker_ = std::move(other.worker_);
  address_ = std::exchange(other.address_, nullptr);
  address_length_ = other.address_length_;
  return *this;
}

std::vector<char> local_address::serialize() const {
  std::vector<char> buffer;
  auto it = std::back_inserter(buffer);
  detail::serialize(address_length_, it);
  std::copy_n(reinterpret_cast<char const *>(address_), address_length_, it);
  return buffer;
}

const ucp_address_t *local_address::get_address() const { return address_; }

size_t local_address::get_length() const { return address_length_; }

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

size_t remote_address::get_length() const { return address_.size(); }

} // namespace ucxpp