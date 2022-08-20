cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
project(ucxpp)
set(CMAKE_CXX_STANDARD 20)

add_compile_options(-Wall -Wextra -pedantic -Werror)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Threads REQUIRED)
find_package(ucx REQUIRED)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_LENGTH)
add_definitions("-DSOURCE_PATH_LENGTH=${SOURCE_PATH_LENGTH}")

option(UCXPP_BUILD_EXAMPLES "Build examples" ON)

include_directories(include)

set(UCXPP_SOURCE_FILES
  src/socket/channel.cc
  src/socket/event_loop.cc
  src/socket/tcp_connection.cc
  src/socket/tcp_listener.cc
  src/awaitable.cc
  src/acceptor.cc
  src/connector.cc
  src/context.cc
  src/worker.cc
  src/endpoint.cc
  src/address.cc
  src/memory.cc
)

add_library(ucxpp STATIC ${UCXPP_SOURCE_FILES})
set(UCXPP_LINK_LIBRARIES ucx::ucp ucx::uct ucx::ucs Threads::Threads)
list(APPEND
  UCXPP_COMPILE_OPTIONS
  PUBLIC
  -flto
  PUBLIC
  -fno-rtti
)
list(APPEND
  UCXPP_LINK_OPTIONS
  PUBLIC
  -flto
  PUBLIC
  -fno-rtti
)

if (CMAKE_CXX_COMPILER_ID MATCHES Clang)
  list(APPEND
    UCXPP_COMPILE_OPTIONS
    PUBLIC
    -stdlib=libc++
    PUBLIC
    -fcoroutines-ts
  )
  list(APPEND
    UCXPP_LINK_OPTIONS
    PUBLIC
    -stdlib=libc++
  )
elseif (CMAKE_CXX_COMPILER_ID MATCHES GNU)
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "11") 
    list(APPEND
      UCXPP_COMPILE_OPTIONS
      PUBLIC
      -fcoroutines
    )
  endif ()
endif ()
if (UCXPP_COMPILE_OPTIONS)
  target_compile_options(ucxpp ${UCXPP_COMPILE_OPTIONS})
endif ()
if (UCXPP_LINK_OPTIONS)
  target_link_options(ucxpp ${UCXPP_LINK_OPTIONS})
endif ()
target_link_libraries(ucxpp ${UCXPP_LINK_LIBRARIES})

set(UCXPP_EXAMPLES helloworld perftest)
if (UCXPP_BUILD_EXAMPLES)
  foreach (EXAMPLE ${UCXPP_EXAMPLES})
    add_executable(${EXAMPLE} examples/${EXAMPLE}.cc)
    target_link_libraries(${EXAMPLE} ucxpp) 
  endforeach ()
endif ()

include(GNUInstallDirs)
install(TARGETS ucxpp EXPORT ucxpp ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ucxpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
