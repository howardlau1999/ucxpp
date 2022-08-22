#pragma once

#include <functional>
#include <memory>
#include <ucs/type/status.h>
#include <unordered_map>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/address.h"
#include "ucxpp/awaitable.h"
#include "ucxpp/context.h"

namespace ucxpp {

/**
 * @brief Abstraction for a UCX worker.
 *
 */
class worker : public std::enable_shared_from_this<worker> {
  friend class local_address;
  friend class endpoint;
  ucp_worker_h worker_;
  std::shared_ptr<context> ctx_;
  int event_fd_;

public:
  /**
   * @brief Construct a new worker object
   *
   * @param ctx UCX context
   */
  worker(std::shared_ptr<context> ctx);

  /**
   * @brief Get the event fd for the worker. The wakeup feature must be enabled
   * for this to work.
   *
   * @return int
   */
  int event_fd() const;

  /**
   * @brief Get the worker's context object
   *
   * @return std::shared_ptr<context> The worker's context object
   */
  std::shared_ptr<context> context_ptr();

  /**
   * @brief Get the worker's UCX address
   *
   * @return local_address The worker's UCX address
   */
  local_address get_address();

  /**
   * @brief Get the worker's native UCX handle
   *
   * @return ucp_worker_h The worker's native UCX handle
   */
  ucp_worker_h handle();

  /**
   * @brief Progress the worker
   *
   * @return true If progress was made
   * @return false If no progress was made
   */
  bool progress() const;

  /**
   * @brief Wait for an event on the worker. It should be called only after a
   * call to progress() returns false.
   *
   */
  void wait() const;

  /**
   * @brief Arm the worker for next event notification.
   *
   */
  bool arm() const;

  /**
   * @brief Flush the worker
   *
   * @return worker_flush_awaitable A coroutine that returns when the worker is
   * flushed
   */
  worker_flush_awaitable flush();

  ~worker();
};

} // namespace ucxpp