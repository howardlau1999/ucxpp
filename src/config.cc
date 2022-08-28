#include "ucxpp/config.h"

#include "ucxpp/error.h"
namespace ucxpp {

config::config(char const *env_prefix, char const *filename) {
  check_ucs_status(::ucp_config_read(env_prefix, filename, &config_),
                   "failed to read ucp config");
}

config::~config() { ::ucp_config_release(config_); }

ucp_config_t *config::handle() const { return config_; }

void config::modify(char const *name, char const *value) {
  check_ucs_status(::ucp_config_modify(config_, name, value),
                   "failed to modify ucp config");
}

void config::print() const {
  ::ucp_config_print(config_, stdout, nullptr, UCS_CONFIG_PRINT_CONFIG);
}

} // namespace ucxpp