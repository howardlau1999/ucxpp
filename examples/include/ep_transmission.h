#pragma once

#include "socket/tcp_connection.h"

#include <ucxpp/address.h>
#include <ucxpp/endpoint.h>
#include <ucxpp/task.h>

namespace ucxpp {

/**
 * @brief Accept a UCX endpoint from a remote peer
 *
 * @param conncetion The TCP connection
 * @param worker The UCX worker
 * @return task<std::shared_ptr<endpoint>> A coroutine that returns the
 * accepted endpoint
 */
task<std::shared_ptr<endpoint>>
from_tcp_connection(socket::tcp_connection &conncetion,
                    std::shared_ptr<worker> worker);

/**
 * @brief Send the address to a remote peer
 *
 * @param connection The TCP connection to send the address over
 * @return task<void> A coroutine
 */
task<void> send_address(local_address const &address,
                        socket::tcp_connection &connection);

} // namespace ucxpp