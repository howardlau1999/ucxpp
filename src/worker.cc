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
#include "ucxpp/socket/channel.h"

#include "ucxpp/detail/debug.h"

namespace ucxpp {

worker::worker(std::shared_ptr<context> ctx) : ctx_(ctx) {
  ucp_worker_params_t worker_params;
  worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
  worker_params.thread_mode = UCS_THREAD_MODE_MULTI;
  check_ucs_status(::ucp_worker_create(ctx->context_, &worker_params, &worker_),
                   "failed to create ucp worker");
}

worker::worker(std::shared_ptr<context> ctx,
               std::shared_ptr<socket::event_loop> loop)
    : worker(ctx) {
  if (!(ctx_->features() & UCP_FEATURE_WAKEUP)) {
    throw std::runtime_error("context does not support wakeup");
  }
  int efd;
  check_ucs_status(::ucp_worker_get_efd(worker_, &efd),
                   "failed to get ucp worker event fd");
  event_channel_ = std::make_shared<socket::channel>(efd);
  check_ucs_status(::ucp_worker_arm(worker_), "failed to arm ucp worker");
  register_loop(loop);
}

void worker::register_loop(std::shared_ptr<socket::event_loop> loop) {
  event_channel_->set_event_loop(loop);
  event_channel_->set_readable_callback(
      [&worker = *this, event_channel = std::weak_ptr<ucxpp::socket::channel>(
                            event_channel_)]() {
        while (worker.progress()) {
          continue;
        }
        auto event_channel_ptr = event_channel.lock();
        if (event_channel_ptr) {
          event_channel_ptr->wait_readable();
        }
      });
  event_channel_->wait_readable();
}

std::shared_ptr<socket::channel> worker::event_channel() const {
  return event_channel_;
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

worker_flush_awaitable worker::flush() {
  return worker_flush_awaitable(worker_);
}

worker::~worker() {
  if (event_channel_) {
    event_channel_->set_event_loop(nullptr);
  };
  ::ucp_worker_destroy(worker_);
}

} // namespace ucxpp