#include <algorithm>
#include <endian.h>
#include <iomanip>
#include <iostream>
#include <memory>

#include <ucp/api/ucp_def.h>

#include "ucxpp/context.h"
#include "ucxpp/endpoint.h"
#include "ucxpp/memory.h"
#include "ucxpp/socket/channel.h"
#include <ucxpp/ucxpp.h>

constexpr ucp_tag_t kTestTag = 0xFD709394UL;
constexpr ucp_tag_t kBellTag = 0xbe11be11UL;

ucxpp::task<std::pair<uint64_t, ucxpp::remote_memory_handle>>
receive_mr(std::shared_ptr<ucxpp::endpoint> ep) {
  using std::cout;
  using std::endl;
  uint64_t remote_addr;
  co_await ep->stream_recv(&remote_addr, sizeof(remote_addr));
  remote_addr = ::be64toh(remote_addr);
  size_t rkey_length;
  co_await ep->stream_recv(&rkey_length, sizeof(rkey_length));
  rkey_length = ::be64toh(rkey_length);
  std::vector<char> rkey_buffer(rkey_length);
  size_t rkey_recved = 0;
  while (rkey_recved < rkey_length) {
    auto n = co_await ep->stream_recv(&rkey_buffer[rkey_recved],
                                      rkey_length - rkey_recved);
    rkey_recved += n;
  }
  co_return std::make_pair(remote_addr,
                           ucxpp::remote_memory_handle(ep, rkey_buffer.data()));
}

ucxpp::task<void> client(ucxpp::connector connector) {
  auto ep = co_await connector.connect();
  ep->print();
  char buffer[6];

  /* Tag Send/Recv */
  auto [n, sender_tag] =
      co_await ep->tag_recv(buffer, sizeof(buffer), kTestTag);
  std::cout << "Received " << n << " bytes from " << std::hex << sender_tag
            << std::dec << ": " << buffer << std::endl;
  std::copy_n("world", 6, buffer);
  co_await ep->tag_send(buffer, sizeof(buffer), kTestTag);

  /* Stream Send/Recv */
  n = co_await ep->stream_recv(buffer, sizeof(buffer));
  std::cout << "Received " << n << " bytes: " << buffer << std::endl;
  std::copy_n("world", 6, buffer);
  co_await ep->stream_send(buffer, sizeof(buffer));

  /* RMA Get/Put */
  auto local_mr = ucxpp::local_memory_handle::register_mem(
      ep->worker_ptr()->context_ptr(), buffer, sizeof(buffer));
  auto [remote_addr, remote_mr] = co_await receive_mr(ep);
  std::cout << "Remote addr: 0x" << std::hex << remote_addr << std::dec
            << std::endl;
  co_await remote_mr.get(buffer, sizeof(buffer), remote_addr);
  std::cout << "Read from server: " << buffer << std::endl;
  std::copy_n("world", 6, buffer);
  co_await remote_mr.put(buffer, sizeof(buffer), remote_addr);
  std::cout << "Wrote to server: " << buffer << std::endl;
  size_t bell;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);

  /* Atomic */
  uint64_t local_value = 1;
  uint64_t reply_value = 0;
  auto [atomic_raddr, atomic_mr] = co_await receive_mr(ep);

  /* Fetch and Add */
  co_await atomic_mr.atomic_fetch_add(atomic_raddr, local_value, reply_value);
  std::cout << "Fetched and added on server: " << reply_value << std::endl;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);
  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);

  /* Compare and Swap */
  local_value = reply_value + local_value;
  reply_value = 456;
  co_await atomic_mr.atomic_compare_swap(atomic_raddr, local_value,
                                         reply_value);
  std::cout << "Compared and swapped on server: " << reply_value << std::endl;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);
  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);

  /* Swap */
  local_value = 123;
  co_await atomic_mr.atomic_swap(atomic_raddr, local_value, reply_value);
  std::cout << "Swapped on server: " << reply_value << std::endl;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);
  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);

  /* Fetch and And */
  local_value = 0xF;
  co_await atomic_mr.atomic_fetch_and(atomic_raddr, local_value, reply_value);
  std::cout << "Fetched and anded on server: " << reply_value << std::endl;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);
  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);

  /* Fetch and Or */
  local_value = 0xF;
  co_await atomic_mr.atomic_fetch_or(atomic_raddr, local_value, reply_value);
  std::cout << "Fetched and ored on server: " << reply_value << std::endl;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);
  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);

  /* Fetch and Xor */
  local_value = 0xF;
  co_await atomic_mr.atomic_fetch_xor(atomic_raddr, local_value, reply_value);
  std::cout << "Fetched and xored on server: " << reply_value << std::endl;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);
  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);

  co_await ep->flush();
  co_await ep->close();

  co_return;
}

ucxpp::task<void> send_mr(std::shared_ptr<ucxpp::endpoint> ep, void *address,
                          ucxpp::local_memory_handle const &local_mr) {
  auto packed_rkey = local_mr.pack_rkey();
  auto rkey_length = ::htobe64(packed_rkey.get_length());
  auto remote_addr = ::htobe64(reinterpret_cast<uint64_t>(address));
  co_await ep->stream_send(&remote_addr, sizeof(remote_addr));
  co_await ep->stream_send(&rkey_length, sizeof(rkey_length));
  co_await ep->stream_send(packed_rkey.get_buffer(), packed_rkey.get_length());
  co_return;
}

ucxpp::task<void> handle_endpoint(std::shared_ptr<ucxpp::endpoint> ep) {
  ep->print();
  char buffer[6] = "Hello";

  /* Tag Send/Recv */
  co_await ep->tag_send(buffer, sizeof(buffer), kTestTag);
  auto [n, sender_tag] =
      co_await ep->tag_recv(buffer, sizeof(buffer), kTestTag);
  std::cout << "Received " << n << " bytes from " << std::hex << sender_tag
            << std::dec << ": " << buffer << std::endl;

  /* Stream Send/Recv */
  std::copy_n("Hello", 6, buffer);
  co_await ep->stream_send(buffer, sizeof(buffer));
  n = co_await ep->stream_recv(buffer, sizeof(buffer));
  std::cout << "Received " << n << " bytes: " << buffer << std::endl;

  /* RMA Get/Put */
  std::copy_n("Hello", 6, buffer);
  auto local_mr = ucxpp::local_memory_handle::register_mem(
      ep->worker_ptr()->context_ptr(), buffer, sizeof(buffer));
  co_await send_mr(ep, buffer, local_mr);

  size_t bell;
  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);
  std::cout << "Written by client: " << buffer << std::endl;

  /* Atomic */
  uint64_t value = 42;
  auto atomic_mr = ucxpp::local_memory_handle::register_mem(
      ep->worker_ptr()->context_ptr(), &value, sizeof(value));
  co_await send_mr(ep, &value, atomic_mr);

  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);
  std::cout << "Fetched and added by client: " << value << std::endl;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);

  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);
  std::cout << "Compared and Swapped by client: " << value << std::endl;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);

  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);
  std::cout << "Swapped by client: " << value << std::endl;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);

  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);
  std::cout << "Fetched and Anded by client: " << value << std::endl;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);

  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);
  std::cout << "Fetched and Ored by client: " << value << std::endl;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);

  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);
  std::cout << "Fetched and Xored by client: " << value << std::endl;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);

  co_await ep->flush();
  co_await ep->close();

  co_return;
}

ucxpp::task<void> server(ucxpp::acceptor acceptor) {
  while (true) {
    auto ep = co_await acceptor.accept();
    handle_endpoint(ep).detach();
  }
  co_return;
}

int main(int argc, char *argv[]) {
  auto ctx = ucxpp::context::builder()
                 .enable_stream()
                 .enable_tag()
                 .enable_rma()
                 .enable_amo64()
                 .enable_wakeup()
                 .build();
  auto loop = ucxpp::socket::event_loop::new_loop();
  auto worker = std::make_shared<ucxpp::worker>(ctx, loop);
  if (argc == 2) {
    auto listener = std::make_shared<ucxpp::socket::tcp_listener>(
        loop, "0.0.0.0", std::stoi(argv[1]));
    auto acceptor = ucxpp::acceptor(worker, listener);
    server(std::move(acceptor)).detach();
  } else if (argc == 3) {
    auto connector =
        ucxpp::connector(worker, loop, argv[1], std::stoi(argv[2]));
    client(std::move(connector)).detach();
  } else {
    std::cout << "Usage: " << argv[0] << " <host> <port>" << std::endl;
  }
  bool close_triggered;
  while (worker.use_count() > 1) {
    loop->poll(close_triggered);
  }
  loop->close();
  loop->poll(close_triggered);
  return 0;
}