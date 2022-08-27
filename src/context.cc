#include "ucxpp/context.h"

#include <cstdint>
#include <cstdio>
#include <ucs/config/types.h>

#include <ucp/api/ucp.h>

#include "ucxpp/awaitable.h"
#include "ucxpp/error.h"

namespace ucxpp {

context::builder::builder() : features_(0), print_config_(false) {}

std::shared_ptr<context> context::builder::build() {
  return std::make_shared<context>(features_, print_config_, enable_mt_);
}

context::builder &context::builder::enable_print_config() {
  print_config_ = true;
  return *this;
}

context::builder &context::builder::enable_wakeup() {
  features_ |= UCP_FEATURE_WAKEUP;
  return *this;
}

context::builder &context::builder::enable_tag() {
  features_ |= UCP_FEATURE_TAG;
  return *this;
}

context::builder &context::builder::enable_stream() {
  features_ |= UCP_FEATURE_STREAM;
  return *this;
}

context::builder &context::builder::enable_am() {
  features_ |= UCP_FEATURE_AM;
  return *this;
}

context::builder &context::builder::enable_rma() {
  features_ |= UCP_FEATURE_RMA;
  return *this;
}

context::builder &context::builder::enable_amo32() {
  features_ |= UCP_FEATURE_AMO32;
  return *this;
}

context::builder &context::builder::enable_amo64() {
  features_ |= UCP_FEATURE_AMO64;
  return *this;
}

context::builder &context::builder::enable_mt() {
  enable_mt_ = true;
  return *this;
}

context::context(uint64_t features, bool print_config, bool enable_mt)
    : features_(features) {
  ucp_config_t *config;
  check_ucs_status(::ucp_config_read(NULL, NULL, &config),
                   "failed to read ucp config");
  ucp_params_t ucp_params;
  ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
  ucp_params.features = features;
  if (enable_mt) {
    ucp_params.field_mask |= UCP_PARAM_FIELD_MT_WORKERS_SHARED;
    ucp_params.mt_workers_shared = 1;
  }
  check_ucs_status(::ucp_init(&ucp_params, config, &context_),
                   "failed to init ucp");
  if (print_config) {
    ::ucp_config_print(config, stdout, nullptr, UCS_CONFIG_PRINT_CONFIG);
  }
  ::ucp_config_release(config);
}

uint64_t context::features() const { return features_; }

ucp_context_h context::handle() const { return context_; }

context::~context() { ::ucp_cleanup(context_); }

} // namespace ucxpp