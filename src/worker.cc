#include "ucxpp/worker.h"

#include <cassert>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <ucs/type/status.h>
#include <ucs/type/thread_mode.h>
#include <unordered_map>

#include <ucp/api/ucp.h>

#include "ucxpp/address.h"
#include "ucxpp/awaitable.h"
#include "ucxpp/error.h"

#include "ucxpp/detail/debug.h"

namespace ucxpp {

worker::worker(std::shared_ptr<context> ctx) : ctx_(ctx), event_fd_(-1) {
  ucp_worker_params_t worker_params;
  worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
  worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;
  check_ucs_status(::ucp_worker_create(ctx->context_, &worker_params, &worker_),
                   "failed to create ucp worker");
  if (ctx_->features() & UCP_FEATURE_WAKEUP) {
    check_ucs_status(::ucp_worker_get_efd(worker_, &event_fd_),
                     "failed to get ucp worker event fd");
  }
}

int worker::event_fd() const {
  assert(event_fd_ != -1);
  return event_fd_;
}

std::shared_ptr<context> worker::context_ptr() { return ctx_; }

local_address worker::get_address() {
  ucp_address_t *address;
  size_t address_length;
  check_ucs_status(::ucp_worker_get_address(worker_, &address, &address_length),
                   "failed to get address");
  return ucxpp::local_address(shared_from_this(), address, address_length);
}

ucp_worker_h worker::handle() { return worker_; }

bool worker::progress() const { return ::ucp_worker_progress(worker_); }

void worker::wait() const {
  check_ucs_status(::ucp_worker_wait(worker_), "failed to wait worker");
}

bool worker::arm() const {
  auto status = ::ucp_worker_arm(worker_);
  if (status == UCS_ERR_BUSY) {
    return false;
  }
  check_ucs_status(status, "failed to arm worker");
  return true;
}

worker_flush_awaitable worker::flush() {
  return worker_flush_awaitable(this->shared_from_this());
}

worker::~worker() { ::ucp_worker_destroy(worker_); }

} // namespace ucxpp