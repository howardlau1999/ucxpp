cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
project(rdmapp)
set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Threads REQUIRED)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_LENGTH)
add_definitions("-DSOURCE_PATH_LENGTH=${SOURCE_PATH_LENGTH}")

include_directories(include)

set(UCXPP_SOURCE_FILES
  src/socket/channel.cc
  src/socket/event_loop.cc
  src/socket/tcp_connection.cc
  src/socket/tcp_listener.cc
  src/context.cc
  src/worker.cc
  src/endpoint.cc
  src/address.cc
)

add_library(ucxpp STATIC ${UCXPP_SOURCE_FILES})
target_link_libraries(ucxpp ucp ucs uct Threads::Threads)

if (CMAKE_CXX_COMPILER_ID MATCHES Clang)
  target_compile_options(ucxpp
    PUBLIC
    -stdlib=libc++
    -fcoroutines-ts
  )
elseif (CMAKE_CXX_COMPILER_ID MATCHES GNU)
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "11") 
    target_compile_options(ucxpp
      PUBLIC
      -fcoroutines
    )
  endif ()
endif ()

include(GNUInstallDirs)
install(TARGETS ucxpp EXPORT ucxpp ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ucxpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})