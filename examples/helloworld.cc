#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>

#include <ucp/api/ucp_def.h>

#include "ucxpp/context.h"
#include <ucxpp/ucxpp.h>

constexpr ucp_tag_t kTestTag = 0xFD709394;

ucxpp::task<void> client(ucxpp::connector &connector) {
  auto ep = co_await connector.connect();
  ep->print();
  char buffer[6];

  /* Tag Send/Recv */
  auto [n, sender_tag] =
      co_await ep->tag_recv(buffer, sizeof(buffer), kTestTag);
  std::cout << "Received " << n << " bytes from " << std::hex << sender_tag
            << std::dec << ": " << buffer << std::endl;
  co_await ep->tag_send(buffer, sizeof(buffer), kTestTag);

  /* Stream Send/Recv */
  n = co_await ep->stream_recv(buffer, sizeof(buffer));
  std::cout << "Received " << n << " bytes: " << buffer << std::endl;
  co_await ep->stream_send(buffer, sizeof(buffer));

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
  co_await ep->stream_send(buffer, sizeof(buffer));
  n = co_await ep->stream_recv(buffer, sizeof(buffer));
  std::cout << "Received " << n << " bytes: " << buffer << std::endl;

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
    server(acceptor);
  } else if (argc == 3) {
    auto connector =
        ucxpp::connector(worker, loop, argv[1], std::stoi(argv[2]));
    client(connector);
  } else {
    std::cout << "Usage: " << argv[0] << " <host> <port>" << std::endl;
  }
  stopped = true;
  loop->close();
  looper.join();
  progresser.join();
  return 0;
}