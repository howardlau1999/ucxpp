#include <algorithm>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <cstdio>
#include <cstring>
#include <endian.h>
#include <memory>
#include <optional>
#include <pthread.h>
#include <ratio>
#include <sched.h>
#include <thread>
#include <vector>

#include <ucxpp/ucxpp.h>

enum class test_category {
  tag,
  stream,
  rma,
};

enum class test_type {
  bw,
  lat,
};

struct perf_context {
  std::pair<test_category, test_type> test = {test_category::tag,
                                              test_type::bw};
  size_t concurrency = 1;
  size_t iterations = 10000000;
  size_t message_size = 8;
  size_t warmup_iterations = 10000;
  bool epoll = false;
  std::string server_address;
  uint16_t server_port = 8888;
  std::optional<size_t> core;
};

constexpr ucp_tag_t k_test_tag = 0xFD709394;

bool g_connected = false;

static size_t g_counter = 0;
static size_t g_last_counter = 0;
auto g_last_tick = std::chrono::system_clock::now();
decltype(g_last_tick) g_start;

void reset_report() {
  g_counter = 0;
  g_last_counter = 0;
  g_last_tick = std::chrono::system_clock::now();
}

void print_report(bool final = false) {
  auto counter = g_counter;
  auto tick = std::chrono::system_clock::now();
  auto elapsed = tick - g_last_tick;
  if (final) [[unlikely]] {
    std::chrono::duration<double> total_elapsed = tick - g_start;
    ::fprintf(stdout, "----- Finished -----");
    ::fprintf(stdout, "Total elapsed: %.3fs\n", total_elapsed.count());
    ::fprintf(stdout, "Average IOPS: %zu\n",
              static_cast<size_t>(counter / total_elapsed.count()));
  } else if (elapsed.count() > 1000000000) [[unlikely]] {
    ::fprintf(stdout, "Current iterations: %zu, IOPS: %zu\n", counter,
              (counter - g_last_counter) * 1000000000 / elapsed.count());
    g_last_counter = counter;
    g_last_tick = tick;
  }
}

static void bind_cpu(int core) {
  cpu_set_t cpuset;
  if (auto rc =
          ::pthread_getaffinity_np(::pthread_self(), sizeof(cpuset), &cpuset);
      rc != 0) {
    ::perror("failed to get original affinity");
    return;
  }
  if (!CPU_ISSET(core, &cpuset)) {
    ::fprintf(stderr, "core %d is not in affinity mask\n", core);
    return;
  }
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  if (auto rc =
          ::pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
      rc != 0) {
    ::perror("failed to set affinity");
    return;
  }
}

ucxpp::task<void> sender(std::shared_ptr<ucxpp::endpoint> ep,
                         size_t &iterations, bool warmup,
                         perf_context const &perf) {
  auto [buffer, local_mr] = ucxpp::local_memory_handle::allocate_mem(
      ep->worker_ptr()->context_ptr(), perf.message_size);
  auto total_iterations = warmup ? perf.warmup_iterations : perf.iterations;
  while (iterations < total_iterations) {
    co_await ep->tag_send(buffer, perf.message_size, k_test_tag);
    iterations++;
    print_report();
  }
}

ucxpp::task<void> client(ucxpp::connector connector, perf_context const &perf) {
  auto ep = co_await connector.connect();
  ep->print();
  g_connected = true;

  ::fprintf(stderr, "Warming up...\n");
  {
    auto tasks = std::vector<ucxpp::task<void>>();
    for (size_t i = 0; i < perf.concurrency; ++i) {
      tasks.emplace_back(sender(ep, g_counter, true, perf));
    }
    for (auto &task : tasks) {
      co_await task;
    }
  }

  reset_report();
  g_start = std::chrono::system_clock::now();
  ::fprintf(stderr, "Running...\n");
  {
    auto tasks = std::vector<ucxpp::task<void>>();
    for (size_t i = 0; i < perf.concurrency; ++i) {
      tasks.emplace_back(sender(ep, g_counter, false, perf));
    }
    for (auto &task : tasks) {
      co_await task;
    }
  }
  print_report(true);

  co_await ep->flush();
  co_await ep->close();

  co_return;
}

ucxpp::task<void> receiver(std::shared_ptr<ucxpp::endpoint> ep,
                           size_t &iterations, bool warmup,
                           perf_context const &perf) {
  auto [buffer, local_mr] = ucxpp::local_memory_handle::allocate_mem(
      ep->worker_ptr()->context_ptr(), perf.message_size);
  auto total_iterations = warmup ? perf.warmup_iterations : perf.iterations;
  while (iterations < total_iterations) {
    co_await ep->tag_recv(buffer, perf.message_size, k_test_tag);
    iterations++;
    print_report();
  }
}

ucxpp::task<void> server(ucxpp::acceptor acceptor, perf_context const &perf) {
  auto ep = co_await acceptor.accept();
  ep->print();
  g_connected = true;

  ::fprintf(stderr, "Warming up...\n");
  {
    auto tasks = std::vector<ucxpp::task<void>>();
    for (size_t i = 0; i < perf.concurrency; ++i) {
      tasks.emplace_back(receiver(ep, g_counter, true, perf));
    }
    for (auto &task : tasks) {
      co_await task;
    }
  }

  ::fprintf(stderr, "Running...\n");
  reset_report();
  g_start = std::chrono::system_clock::now();
  {
    std::vector<ucxpp::task<void>> tasks;
    for (size_t i = 0; i < perf.concurrency; ++i) {
      tasks.emplace_back(receiver(ep, g_counter, false, perf));
    }
    for (auto &task : tasks) {
      co_await task;
    }
  }
  print_report(true);

  co_await ep->flush();
  co_await ep->close();

  co_return;
}

void print_usage(char const *argv0) {
  ::fprintf(stderr,
            "Usage: %s [options] \n-c\tSpecify the core\n"
            "-o\tSpecifies concurrent requests (default: 1)\n"
            "-n\tSpecifies number of iterations (default: 1000000)\n"
            "-s\tSpecifies message size (default: 8)\n"
            "-w\tSpecifies number of warmup iterations (default: 10000)\n"
            "-e\tUse epoll for worker progress (default: false)\n"
            "-p\tServer port (default 8888)\n",
            argv0);
}

int main(int argc, char *argv[]) {
  std::ios_base::sync_with_stdio(false);
  auto args = std::vector<std::string>(argv + 1, argv + argc);
  perf_context perf;
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "-h") {
      print_usage(argv[0]);
      return 0;
    } else if (args[i] == "-c") {
      perf.core = std::stoi(args[++i]);
    } else if (args[i] == "-o") {
      perf.concurrency = std::stoul(args[++i]);
    } else if (args[i] == "-s") {
      perf.message_size = std::stoul(args[++i]);
    } else if (args[i] == "-n") {
      perf.iterations = std::stoul(args[++i]);
    } else if (args[i] == "-w") {
      perf.warmup_iterations = std::stoul(args[++i]);
    } else if (args[i] == "-e") {
      perf.epoll = true;
    } else if (args[i] == "-p") {
      perf.server_port = std::stoul(args[++i]);
    } else if (args[i][0] == '-') {
      ::fprintf(stderr, "unknown option: %s\n", args[i].c_str());
      return 1;
    } else {
      perf.server_address = args[i];
    }
  }
  auto ctx = [&]() {
    auto builder = ucxpp::context::builder();
    builder.enable_tag();
    if (perf.epoll) {
      builder.enable_wakeup();
    }
    return builder.build();
  }();
  auto loop = ucxpp::socket::event_loop::new_loop();
  auto worker = [&]() {
    if (perf.epoll) {
      return std::make_shared<ucxpp::worker>(ctx, loop);
    }
    return std::make_shared<ucxpp::worker>(ctx);
  }();
  if (perf.core.has_value()) {
    bind_cpu(perf.core.value());
  } else {
    ::fprintf(stderr,
              "Warning: no core specified, using all cores available\n");
  }

  if (perf.server_address.empty()) {
    auto listener = std::make_shared<ucxpp::socket::tcp_listener>(
        loop, "0.0.0.0", perf.server_port);
    auto acceptor = ucxpp::acceptor(worker, listener);
    server(std::move(acceptor), perf).detach();
  } else {
    auto connector =
        ucxpp::connector(worker, loop, perf.server_address, perf.server_port);
    client(std::move(connector), perf).detach();
  }

  if (!perf.epoll) {
    bool dummy;
    while (!g_connected) {
      loop->poll(dummy);
    }
    loop->close();
    loop = nullptr;
    while (worker.use_count() > 1) {
      worker->progress();
    }
  } else {
    bool dummy = false;
    while (worker.use_count() > 1) {
      loop->poll(dummy);
    }
    loop->close();
    loop->poll(dummy);
  }

  return 0;
}
