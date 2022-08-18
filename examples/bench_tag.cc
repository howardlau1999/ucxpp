#include <algorithm>
#include <atomic>
#include <endian.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

#include <ucp/api/ucp_def.h>

#include "ucxpp/context.h"
#include "ucxpp/memory.h"
#include "ucxpp/task.h"
#include <ucxpp/ucxpp.h>

constexpr ucp_tag_t kTestTag = 0xFD709394;
constexpr size_t kConcurrency = 32;

size_t gMsgSize = 65536;
static std::atomic<size_t> gCounter = 0;

ucxpp::task<void> sender(std::shared_ptr<ucxpp::endpoint> ep) {
  std::vector<char> buffer(gMsgSize);
  while (true) {
    co_await ep->tag_send(&buffer[0], gMsgSize, kTestTag);
    gCounter.fetch_add(1, std::memory_order_seq_cst);
  }
}

ucxpp::task<void> client(ucxpp::connector connector) {
  auto ep = co_await connector.connect();
  ep->print();

  {
    std::vector<ucxpp::task<void>> tasks;
    for (size_t i = 0; i < kConcurrency; ++i) {
      tasks.emplace_back(sender(ep));
    }
  }

  co_await ep->flush();
  co_await ep->close();

  co_return;
}

ucxpp::task<void> receiver(std::shared_ptr<ucxpp::endpoint> ep) {
  std::vector<char> buffer(gMsgSize);

  while (true) {
    co_await ep->tag_recv(&buffer[0], gMsgSize, kTestTag);
    gCounter.fetch_add(1, std::memory_order_seq_cst);
  }
}

ucxpp::task<void> server(ucxpp::acceptor acceptor) {
  auto ep = co_await acceptor.accept();
  ep->print();

  {
    std::vector<ucxpp::task<void>> tasks;
    for (size_t i = 0; i < kConcurrency; ++i) {
      tasks.emplace_back(receiver(ep));
    }
  }

  co_await ep->flush();
  co_await ep->close();

  co_return;
}

int main(int argc, char *argv[]) {
  auto ctx = ucxpp::context::builder().enable_tag().build();
  auto worker = std::make_shared<ucxpp::worker>(ctx);
  auto loop = ucxpp::socket::event_loop::new_loop();
  auto looper = std::thread([loop]() { loop->loop(); });
  bool stopped = false;
  auto reporter = std::thread([&stopped]() {
    using namespace std::literals::chrono_literals;
    while (!stopped) {
      std::cout << "IOPS: " << gCounter.exchange(0, std::memory_order_seq_cst)
                << std::endl;
      std::this_thread::sleep_for(1s);
    }
  });
  if (argc == 3) {
    auto listener = std::make_shared<ucxpp::socket::tcp_listener>(
        loop, "0.0.0.0", std::stoi(argv[1]));
    auto acceptor = ucxpp::acceptor(worker, listener);
    gMsgSize = std::stoi(argv[2]);
    server(std::move(acceptor)).detach();
  } else if (argc == 4) {
    auto connector =
        ucxpp::connector(worker, loop, argv[1], std::stoi(argv[2]));
    gMsgSize = std::stoi(argv[3]);
    client(std::move(connector)).detach();
  } else {
    std::cout << "Usage: " << argv[0] << " <host> <port> <size>" << std::endl;
  }
  while (worker.use_count() > 1) {
    worker->progress();
  }
  stopped = true;
  loop->close();
  looper.join();
  reporter.join();
  return 0;
}
