#include "ucxpp/worker.h"

#include <memory>

#include <ucp/api/ucp.h>

#include "ucxpp/address.h"
#include "ucxpp/error.h"

#include "ucxpp/detail/debug.h"

namespace ucxpp {

worker::worker(std::shared_ptr<context> ctx) : ctx_(ctx) {
  ucp_worker_params_t worker_params;
  worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
  worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;
  check_ucs_status(::ucp_worker_create(ctx->context_, &worker_params, &worker_),
                   "failed to create ucp worker");
}

std::shared_ptr<context> worker::context_ptr() { return ctx_; }

local_address worker::get_address() {
  ucp_address_t *address;
  size_t address_length;
  check_ucs_status(::ucp_worker_get_address(worker_, &address, &address_length),
                   "failed to get address");
  return ucxpp::local_address(shared_from_this(), address, address_length);
}

bool worker::progress() { return ::ucp_worker_progress(worker_); }

void worker::wait() {
  check_ucs_status(::ucp_worker_wait(worker_), "failed to wait worker");
}

worker::~worker() { ::ucp_worker_destroy(worker_); }

} // namespace ucxpp