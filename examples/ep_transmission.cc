#include "ep_transmission.h"

#include <ucxpp/detail/serdes.h>

namespace ucxpp {

task<std::shared_ptr<endpoint>>
from_tcp_connection(socket::tcp_connection &conncetion,
                    std::shared_ptr<worker> worker) {
  size_t address_length_read = 0;
  char address_length_buffer[sizeof(size_t)];
  while (address_length_read < sizeof(size_t)) {
    int n =
        co_await conncetion.recv(address_length_buffer + address_length_read,
                                 sizeof(size_t) - address_length_read);
    if (n == 0) {
      throw std::runtime_error("failed to read address length");
    }
    address_length_read += n;
  }
  size_t address_length;
  char *p = address_length_buffer;
  detail::deserialize(p, address_length);
  std::vector<char> address_buffer(address_length);
  size_t address_read = 0;
  while (address_read < address_length) {
    int n = co_await conncetion.recv(&address_buffer[address_read],
                                     address_length - address_read);
    if (n == 0) {
      throw std::runtime_error("failed to read address");
    }
    address_read += n;
  }
  auto remote_addr = remote_address(std::move(address_buffer));
  co_return std::make_shared<endpoint>(worker, remote_addr);
}

task<void> send_address(local_address const &address,
                        socket::tcp_connection &connection) {
  auto buffer = address.serialize();
  size_t sent = 0;
  while (sent < buffer.size()) {
    int n = co_await connection.send(&buffer[sent], buffer.size() - sent);
    if (n < 0) {
      throw std::runtime_error("send failed");
    }
    sent += n;
  }
  co_return;
}

} // namespace ucxpp