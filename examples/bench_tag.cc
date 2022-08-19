#include <algorithm>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <cstring>
#include <endian.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <vector>

#include <ucp/api/ucp_def.h>

#include "ucxpp/context.h"
#include "ucxpp/memory.h"
#include "ucxpp/task.h"
#include <ucxpp/ucxpp.h>

constexpr ucp_tag_t kTestTag = 0xFD709394;
constexpr size_t kConcurrency = 32;

size_t gMsgSize = 65536;
static std::atomic_size_t gCounter = 0;
static size_t gLastCounter = 0;
auto gLastTick = std::chrono::high_resolution_clock::now();

std::atomic<std::coroutine_handle<>> gCoro;

struct thread_switcher {
  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<> coro) noexcept { gCoro = coro; }
  void await_resume() noexcept {}
};

static void bind_cpu(int core) {
  cpu_set_t cpuset;
  if (auto rc =
          ::pthread_getaffinity_np(::pthread_self(), sizeof(cpuset), &cpuset);
      rc != 0) {
    std::cerr << "pthread_getaffinity_np: " << ::strerror(errno) << std::endl;
    return;
  }
  if (!CPU_ISSET(core, &cpuset)) {
    std::cerr << "core " << core << " is not in affinity mask" << std::endl;
    return;
  }
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  if (auto rc =
          ::pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
      rc != 0) {
    std::cerr << "pthread_setaffinity_np: " << ::strerror(errno) << std::endl;
    return;
  }
}

ucxpp::task<void> sender(std::shared_ptr<ucxpp::endpoint> ep) {
  auto [buffer, local_mr] = ucxpp::local_memory_handle::allocate_mem(
      ep->worker_ptr()->context_ptr(), gMsgSize);
  while (true) {
    co_await ep->tag_send(buffer, gMsgSize, kTestTag);
    gCounter.fetch_add(1, std::memory_order_relaxed);
  }
}

ucxpp::task<void> client(ucxpp::connector connector) {
  auto ep = co_await connector.connect();
  co_await thread_switcher{};
  ep->print();

  {
    for (size_t i = 0; i < kConcurrency; ++i) {
      sender(ep).detach();
    }
  }

  co_await ep->flush();
  co_await ep->close();

  co_return;
}

ucxpp::task<void> receiver(std::shared_ptr<ucxpp::endpoint> ep) {
  auto [buffer, local_mr] = ucxpp::local_memory_handle::allocate_mem(
      ep->worker_ptr()->context_ptr(), gMsgSize);

  while (true) {
    co_await ep->tag_recv(buffer, gMsgSize, kTestTag);
    gCounter.fetch_add(1, std::memory_order_relaxed);
  }
}

ucxpp::task<void> server(ucxpp::acceptor acceptor) {
  auto ep = co_await acceptor.accept();
  co_await thread_switcher{};
  ep->print();

  {
    for (size_t i = 0; i < kConcurrency; ++i) {
      receiver(ep).detach();
    }
  }

  co_await ep->flush();
  co_await ep->close();

  co_return;
}

int main(int argc, char *argv[]) {
  auto ctx = ucxpp::context::builder().enable_tag().build();
  auto loop = ucxpp::socket::event_loop::new_loop();
  auto worker = std::make_shared<ucxpp::worker>(ctx);
  bool stopped = false;
  auto looper = std::thread([&]() {
    bind_cpu(0);
    loop->loop();
  });
  auto reporter = std::thread([&stopped]() {
    bind_cpu(0);
    using namespace std::literals::chrono_literals;
    while (!stopped) {
      auto tick = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double> elapsed = tick - gLastTick;
      auto counter = gCounter.load(std::memory_order_relaxed);
      std::cout << "IOPS: " << (counter - gLastCounter) / elapsed.count()
                << std::endl;
      gLastCounter = gCounter;
      gLastTick = tick;
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
  bind_cpu(5);
  while (worker.use_count() > 1) {
    while (worker->progress()) {
      continue;
    }
    if (auto coro = gCoro.load(); coro) {
      gCoro = nullptr;
      coro.resume();
    }
  }
  stopped = true;
  loop->close();
  looper.join();
  reporter.join();
  return 0;
}
