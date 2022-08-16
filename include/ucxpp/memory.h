#pragma once

#include <memory>

#include "ucxpp/context.h"
#include "ucxpp/endpoint.h"

#include "ucxpp/detail/noncopyable.h"

namespace ucxpp {

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
  static local_memory_handle register_mem(std::shared_ptr<context> ctx,
                                          void *address, size_t length);
  static local_memory_handle unpack_mem(std::shared_ptr<endpoint> endpoint,
                                        packed_memory_rkey const &packed_rkey);
  ucp_mem_h handle();
  packed_memory_rkey pack_rkey();
  ~local_memory_handle();
};

class remote_memory_handle : public noncopyable {
  std::shared_ptr<endpoint> endpoint_;
  ucp_rkey_h rkey_;

public:
  remote_memory_handle(std::shared_ptr<endpoint> endpoint,
                       void const *packed_rkey_buffer);
  remote_memory_handle(remote_memory_handle &&other);
  ucp_rkey_h handle();
  ~remote_memory_handle();
};

} // namespace ucxpp