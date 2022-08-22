#pragma once

#include <cstdio>

#ifndef SOURCE_PATH_LENGTH
#define SOURCE_PATH_LENGTH 0
#endif

#define __UCXPP_FILENAME__ (&__FILE__[SOURCE_PATH_LENGTH])

namespace ucxpp {

enum class LogLevel {
  TRACE,
  DEBUG,
  INFO,
  WARN,
  ERROR,
};
}

constexpr static inline ucxpp::LogLevel ucxpp_log_level =
    ucxpp::LogLevel::DEBUG;

#define UCXPP_LOG_TRACE(msg, ...)                                              \
  do {                                                                         \
    if (ucxpp_log_level > ucxpp::LogLevel::TRACE)                              \
      break;                                                                   \
    printf("[TRACE] [%s:%d] " msg "\n", __UCXPP_FILENAME__,                    \
           __LINE__ __VA_OPT__(, ) __VA_ARGS__);                               \
  } while (0)

#define UCXPP_LOG_DEBUG(msg, ...)                                              \
  do {                                                                         \
    if (ucxpp_log_level > ucxpp::LogLevel::DEBUG)                              \
      break;                                                                   \
    printf("[DEBUG] [%s:%d] " msg "\n", __UCXPP_FILENAME__,                    \
           __LINE__ __VA_OPT__(, ) __VA_ARGS__);                               \
  } while (0)

#define UCXPP_LOG_INFO(msg, ...)                                               \
  do {                                                                         \
    if (ucxpp_log_level > ucxpp::LogLevel::INFO)                               \
      break;                                                                   \
    printf("[INFO ] [%s:%d] " msg "\n", __UCXPP_FILENAME__,                    \
           __LINE__ __VA_OPT__(, ) __VA_ARGS__);                               \
  } while (0)

#define UCXPP_LOG_ERROR(msg, ...)                                              \
  do {                                                                         \
    printf("[ERROR] [%s:%d] " msg "\n", __UCXPP_FILENAME__,                    \
           __LINE__ __VA_OPT__(, ) __VA_ARGS__);                               \
  } while (0)
