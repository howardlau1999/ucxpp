#include "ucxpp/memory.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/endpoint.h"
#include "ucxpp/error.h"

namespace ucxpp {

local_memory_handle::local_memory_handle(std::shared_ptr<context> ctx,
                                         ucp_mem_h mem)
    : ctx_(ctx), mem_(mem) {}

local_memory_handle::local_memory_handle(local_memory_handle &&other)
    : ctx_(std::move(other.ctx_)), mem_(std::exchange(other.mem_, nullptr)) {}

local_memory_handle
local_memory_handle::register_mem(std::shared_ptr<context> ctx, void *address,
                                  size_t length) {
  ucp_mem_h mem;
  ucp_mem_map_params_t map_params;
  map_params.address = address;
  map_params.length = length;
  map_params.field_mask =
      UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH;
  check_ucs_status(::ucp_mem_map(ctx->context_, &map_params, &mem),
                   "failed to map memory");

  return local_memory_handle(ctx, mem);
}

std::pair<void *, local_memory_handle>
local_memory_handle::allocate_mem(std::shared_ptr<context> ctx, size_t length) {
  ucp_mem_h mem;
  ucp_mem_attr_t attr;
  ucp_mem_map_params_t map_params;
  map_params.address = nullptr;
  map_params.length = length;
  map_params.flags = UCP_MEM_MAP_ALLOCATE;
  map_params.field_mask =
      UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH;
  check_ucs_status(::ucp_mem_map(ctx->context_, &map_params, &mem),
                   "failed to map memory");
  attr.field_mask = UCP_MEM_ATTR_FIELD_ADDRESS;
  check_ucs_status(::ucp_mem_query(mem, &attr), "failed to get memory address");
  return std::make_pair(attr.address, local_memory_handle(ctx, mem));
}

packed_memory_rkey local_memory_handle::pack_rkey() const {
  void *buffer;
  size_t length;
  check_ucs_status(::ucp_rkey_pack(ctx_->context_, mem_, &buffer, &length),
                   "failed to pack memory");
  return packed_memory_rkey(buffer, length);
}

ucp_mem_h local_memory_handle::handle() const { return mem_; }

local_memory_handle::~local_memory_handle() {
  if (mem_ != nullptr) {
    (void)::ucp_mem_unmap(ctx_->context_, mem_);
  }
}

packed_memory_rkey::packed_memory_rkey(void *buffer, size_t length)
    : buffer_(buffer), length_(length) {}

packed_memory_rkey::packed_memory_rkey(packed_memory_rkey &&other)
    : buffer_(std::exchange(other.buffer_, nullptr)),
      length_(std::exchange(other.length_, 0)) {}

void *packed_memory_rkey::get_buffer() { return buffer_; }

void const *packed_memory_rkey::get_buffer() const { return buffer_; }

size_t packed_memory_rkey::get_length() const { return length_; }

packed_memory_rkey::~packed_memory_rkey() {
  if (buffer_ != nullptr) {
    ::ucp_rkey_buffer_release(buffer_);
  }
}

remote_memory_handle::remote_memory_handle(std::shared_ptr<endpoint> endpoint,
                                           void const *packed_rkey_buffer)
    : endpoint_(endpoint), ep_(endpoint->handle()) {
  check_ucs_status(
      ::ucp_ep_rkey_unpack(endpoint_->handle(), packed_rkey_buffer, &rkey_),
      "failed to unpack memory");
}

remote_memory_handle::remote_memory_handle(remote_memory_handle &&other)
    : endpoint_(std::move(other.endpoint_)),
      ep_(std::exchange(other.ep_, nullptr)),
      rkey_(std::exchange(other.rkey_, nullptr)) {}

std::shared_ptr<endpoint> remote_memory_handle::endpoint_ptr() const {
  return endpoint_;
}

ucp_rkey_h remote_memory_handle::handle() const { return rkey_; }

rma_put_awaitable remote_memory_handle::put(void const *buffer, size_t length,
                                            uint64_t raddr) const {
  return rma_put_awaitable(ep_, buffer, length, raddr, rkey_);
}

rma_get_awaitable remote_memory_handle::get(void *buffer, size_t length,
                                            uint64_t raddr) const {
  return rma_get_awaitable(ep_, buffer, length, raddr, rkey_);
}

rma_put_awaitable remote_memory_handle::write(void const *buffer, size_t length,
                                              uint64_t raddr) const {
  return rma_put_awaitable(ep_, buffer, length, raddr, rkey_);
}

rma_get_awaitable remote_memory_handle::read(void *buffer, size_t length,
                                             uint64_t raddr) const {
  return rma_get_awaitable(ep_, buffer, length, raddr, rkey_);
}

remote_memory_handle::~remote_memory_handle() {
  if (rkey_ != nullptr) {
    ::ucp_rkey_destroy(rkey_);
  }
}

} // namespace ucxpp