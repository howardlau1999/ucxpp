#pragma once

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <ucp/api/ucp.h>

namespace ucxpp {

/**
 * @brief Abstraction of a UCX context
 *
 */
class context : public std::enable_shared_from_this<context> {
  friend class worker;
  friend class local_memory_handle;
  ucp_context_h context_;
  uint64_t features_;

public:
  /**
   * @brief Context builder
   *
   */
  class builder {
    uint64_t features_;
    bool print_config_;
    bool enable_mt_;

  public:
    builder();
    /**
     * @brief Build and return a context object
     *
     * @return std::shared_ptr<context> The built context object
     */
    std::shared_ptr<context> build();

    /**
     * @brief Print the config to stdout when building context
     *
     * @return builder
     */
    builder &enable_print_config();

    /**
     * @brief Enable the wakeup feature
     *
     * @return builder&
     */
    builder &enable_wakeup();

    /**
     * @brief Enable tag-related operations
     *
     * @return builder&
     */
    builder &enable_tag();

    /**
     * @brief Enable stream-related operations
     *
     * @return builder&
     */
    builder &enable_stream();

    /**
     * @brief Enable active message feature
     *
     * @return builder&
     */
    builder &enable_am();

    /**
     * @brief Enable remote memory access feature
     *
     * @return builder&
     */
    builder &enable_rma();

    /**
     * @brief Enable atomic memory operations with 32-bit operands
     *
     * @return builder&
     */
    builder &enable_amo32();

    /**
     * @brief Enable atomic memory operations with 64-bit operands
     *
     * @return builder&
     */
    builder &enable_amo64();

    /**
     * @brief Enable multi-threading
     *
     * @return builder&
     */
    builder &enable_mt();
  };

  /**
   * @brief Construct a new context object
   *
   * @param features Feature flags
   * @param print_config Print the config to stdout
   * @param enable_mt Enable multi-threading
   */
  context(uint64_t features, bool print_config, bool enable_mt);

  /**
   * @brief Get the features of the context
   *
   * @return uint64_t Feature flags
   */
  uint64_t features() const;

  /**
   * @brief Get the native UCX handle of the context
   *
   * @return ucp_context_h The native UCX handle
   */
  ucp_context_h handle() const;

  /**
   * @brief Destroy the context object and release resources
   *
   */
  ~context();
};

} // namespace ucxpp