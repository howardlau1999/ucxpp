#include "ucxpp/context.h"

#include <cstdio>
#include <ucs/config/types.h>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/error.h"

namespace ucxpp {

context::context(bool print_config) {
  ucp_config_t *config;
  check_ucs_status(::ucp_config_read(NULL, NULL, &config),
                   "failed to read ucp config");
  ucp_params_t ucp_params;
  ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
  ucp_params.features =
      UCP_FEATURE_TAG | UCP_FEATURE_WAKEUP | UCP_FEATURE_STREAM;
  check_ucs_status(::ucp_init(&ucp_params, config, &context_),
                   "failed to init ucp");
  if (print_config) {
    ::ucp_config_print(config, stdout, nullptr, UCS_CONFIG_PRINT_CONFIG);
  }
  ::ucp_config_release(config);
}

context::~context() { ::ucp_cleanup(context_); }

} // namespace ucxpp