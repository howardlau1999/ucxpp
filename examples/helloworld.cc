#include <algorithm>
#include <endian.h>
#include <iomanip>
#include <iostream>
#include <memory>

#include <ucp/api/ucp_def.h>

#include "ucxpp/context.h"
#include "ucxpp/memory.h"
#include <ucxpp/ucxpp.h>

constexpr ucp_tag_t kTestTag = 0xFD709394;
constexpr ucp_tag_t kBellTag = 0xbe11be11;

ucxpp::task<void> client(ucxpp::connector &connector) {
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
  uint64_t remote_addr;
  co_await ep->stream_recv(&remote_addr, sizeof(remote_addr));
  remote_addr = ::be64toh(remote_addr);
  size_t rkey_length;
  co_await ep->stream_recv(&rkey_length, sizeof(rkey_length));
  rkey_length = ::be64toh(rkey_length);
  std::vector<char> rkey_buffer(rkey_length);
  int rkey_recved = 0;
  while (rkey_recved < rkey_length) {
    auto n = co_await ep->stream_recv(&rkey_buffer[rkey_recved],
                                      rkey_length - rkey_recved);
    rkey_recved += n;
  }
  auto rkey = ucxpp::remote_memory_handle(ep, rkey_buffer.data());
  std::cout << "Remote addr: 0x" << std::hex << remote_addr << std::dec
            << " Rkey length: " << rkey_length << std::endl;
  co_await ep->rma_get(buffer, sizeof(buffer), remote_addr, rkey.handle());
  std::cout << "Read from server: " << buffer << std::endl;
  std::copy_n("world", 6, buffer);
  co_await ep->rma_put(buffer, sizeof(buffer), remote_addr, rkey.handle());
  std::cout << "Wrote to server: " << buffer << std::endl;
  size_t bell;
  co_await ep->tag_send(&bell, sizeof(bell), kBellTag);
  std::cout << "Sent bell" << std::endl;
  co_return;
}

ucxpp::task<void> server(ucxpp::acceptor &acceptor) {
  auto ep = co_await acceptor.accept();
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
  auto packed_rkey = local_mr.pack_rkey();
  auto rkey_buffer = packed_rkey.get_buffer();
  auto rkey_length = ::htobe64(packed_rkey.get_length());
  auto remote_addr = ::htobe64(reinterpret_cast<uint64_t>(buffer));
  co_await ep->stream_send(&remote_addr, sizeof(remote_addr));
  co_await ep->stream_send(&rkey_length, sizeof(rkey_length));
  co_await ep->stream_send(packed_rkey.get_buffer(), packed_rkey.get_length());

  size_t bell;
  co_await ep->tag_recv(&bell, sizeof(bell), kBellTag);
  std::cout << "Written by client: " << buffer << std::endl;

  co_return;
}

int main(int argc, char *argv[]) {
  auto ctx = ucxpp::context::builder()
                 .enable_stream()
                 .enable_tag()
                 .enable_am()
                 .enable_rma()
                 .enable_wakeup()
                 .build();
  auto worker = std::make_shared<ucxpp::worker>(ctx);
  auto loop = ucxpp::socket::event_loop::new_loop();
  auto looper = std::thread([loop]() { loop->loop(); });
  bool stopped;
  auto progresser = std::thread([worker, &stopped]() {
    while (!stopped) {
      worker->progress();
    }
  });
  if (argc == 2) {
    auto listener = std::make_shared<ucxpp::socket::tcp_listener>(
        loop, "", std::stoi(argv[1]));
    auto acceptor = ucxpp::acceptor(worker, listener);
    auto task = server(acceptor);
    auto fut = task.get_future();
    task.detach();
    fut.get();
  } else if (argc == 3) {
    auto connector =
        ucxpp::connector(worker, loop, argv[1], std::stoi(argv[2]));
    auto task = client(connector);
    auto fut = task.get_future();
    task.detach();
    fut.get();
  } else {
    std::cout << "Usage: " << argv[0] << " <host> <port>" << std::endl;
  }
  stopped = true;
  loop->close();
  looper.join();
  progresser.join();
  return 0;
}