#include "ucxpp/worker.h"

#include <cassert>
#include <functional>
#include <memory>
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

ucp_worker_h worker::handle() { return worker_; }

bool worker::progress() { return ::ucp_worker_progress(worker_); }

void worker::add_pending(ucs_status_ptr_t status,
                         std::function<void(ucs_status_t)> &&callback) {
  std::lock_guard<std::mutex> lock(pending_mutex_);
  pending_.emplace(std::make_pair(status, std::move(callback)));
}

void worker::check_pending() {
  std::lock_guard<std::mutex> lock(pending_mutex_);
  std::unordered_map<ucs_status_ptr_t, std::function<void(ucs_status_t)>>
      inprogress;
  for (auto &pending : pending_) {
    if (auto status = ::ucp_request_check_status(pending.first);
        status != UCS_INPROGRESS) {
      pending.second(status);
    } else {
      inprogress.emplace(std::make_pair(pending.first, pending.second));
    }
  }
  std::swap(inprogress, pending_);
}

void worker::wait() {
  check_ucs_status(::ucp_worker_wait(worker_), "failed to wait worker");
}

send_awaitable worker::flush() {
  return send_awaitable([worker_h = worker_](auto param) {
    return ::ucp_worker_flush_nbx(worker_h, param);
  });
}

worker::~worker() { ::ucp_worker_destroy(worker_); }

} // namespace ucxpp