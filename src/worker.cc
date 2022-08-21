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

worker::worker(std::shared_ptr<context> ctx) : ctx_(ctx), event_fd_(-1) {
  ucp_worker_params_t worker_params;
  worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
  worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;
  check_ucs_status(::ucp_worker_create(ctx->context_, &worker_params, &worker_),
                   "failed to create ucp worker");
  if (ctx_->features() & UCP_FEATURE_WAKEUP) {
    check_ucs_status(::ucp_worker_get_efd(worker_, &event_fd_),
                     "failed to get ucp worker event fd");
    check_ucs_status(::ucp_worker_arm(worker_), "failed to arm ucp worker");
  }
}

worker::worker(std::shared_ptr<context> ctx,
               std::shared_ptr<socket::event_loop> loop)
    : worker(ctx) {
  if (event_fd_ == -1) {
    throw std::runtime_error("wakeup feature is not enabled");
  }
  event_channel_ = std::make_shared<socket::channel>(event_fd_);
  register_loop(loop);
}

int worker::event_fd() const {
  assert(event_fd_ != -1);
  return event_fd_;
}

void worker::register_loop(std::shared_ptr<socket::event_loop> loop) {
  event_channel_->set_event_loop(loop);
  event_channel_->set_readable_callback(
      [&worker = *this, event_channel = std::weak_ptr<ucxpp::socket::channel>(
                            event_channel_)]() {
        while (worker.progress()) {
          continue;
        }
        check_ucs_status(::ucp_worker_arm(worker.worker_),
                         "failed to arm ucp worker");
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

void worker::wait() {
  check_ucs_status(::ucp_worker_wait(worker_), "failed to wait worker");
}

worker_flush_awaitable worker::flush() {
  return worker_flush_awaitable(this->shared_from_this());
}

worker::~worker() {
  if (event_channel_) {
    event_channel_->set_event_loop(nullptr);
  };
  ::ucp_worker_destroy(worker_);
}

} // namespace ucxpp