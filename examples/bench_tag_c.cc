#include <algorithm>
#include <atomic>
#include <endian.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <ucs/type/status.h>
#include <vector>

#include <ucp/api/ucp.h>
#include <ucp/api/ucp_compat.h>
#include <ucp/api/ucp_def.h>

#include "ucxpp/context.h"
#include "ucxpp/memory.h"
#include <ucxpp/ucxpp.h>

constexpr ucp_tag_t kTestTag = 0xFD709394;
constexpr size_t kConcurrency = 32;

size_t gMsgSize = 65536;
static std::atomic<size_t> gCounter = 0;

size_t gOutstandingSend = 0;
size_t gOutstandingRecv = 0;

static ucs_status_t ucp_perf_mem_alloc(std::shared_ptr<ucxpp::context> ctx,
                                       void **address_p, ucp_mem_h *memh_p) {
  ucp_mem_map_params_t params;
  ucp_mem_attr_t attr;
  ucs_status_t status;

  params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                      UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                      UCP_MEM_MAP_PARAM_FIELD_FLAGS;
  params.address = nullptr;
  params.length = gMsgSize;
  params.flags = UCP_MEM_MAP_ALLOCATE;

  status = ::ucp_mem_map(ctx->handle(), &params, memh_p);
  if (status != UCS_OK) {
    return status;
  }

  attr.field_mask = UCP_MEM_ATTR_FIELD_ADDRESS;
  status = ::ucp_mem_query(*memh_p, &attr);
  if (status != UCS_OK) {
    ::ucp_mem_unmap(ctx->handle(), *memh_p);
    return status;
  }

  *address_p = attr.address;
  return UCS_OK;
}

void tag_send_cb(void *request, ucs_status_t, void *) {
  ::ucp_request_release(request);
  gOutstandingSend--;
  gCounter.fetch_add(1, std::memory_order_relaxed);
}

ucxpp::task<void> client(ucxpp::connector connector) {
  auto ep = co_await connector.connect();

  std::thread cli([ep]() {
    ep->print();
    void *buffer = nullptr;
    ucp_mem_h memh;
    ucs_status_t status;
    status =
        ucp_perf_mem_alloc(ep->worker_ptr()->context_ptr(), &buffer, &memh);
    if (status != UCS_OK) {
      std::cerr << "Failed to allocate memory: " << ucs_status_string(status)
                << std::endl;
      return;
    }

    auto ep_h = ep->handle();
    auto worker_h = ep->worker_ptr()->handle();
    ucp_request_param_t send_param;
    send_param.op_attr_mask =
        UCP_OP_ATTR_FLAG_MULTI_SEND | UCP_OP_ATTR_FIELD_CALLBACK;
    send_param.cb.send = tag_send_cb;
    while (true) {
      while (gOutstandingSend >= kConcurrency) {
        ::ucp_worker_progress(worker_h);
      }
      auto request =
          ::ucp_tag_send_nbx(ep_h, buffer, gMsgSize, kTestTag, &send_param);
      if (UCS_PTR_IS_ERR(request)) {
        std::cerr << "Failed to send: "
                  << ucs_status_string(UCS_PTR_STATUS(request)) << std::endl;
        break;
      } else if (UCS_PTR_STATUS(request) == UCS_INPROGRESS) {
        gOutstandingSend++;
      } else {
        gCounter.fetch_add(1, std::memory_order_relaxed);
      }
    }
  });
  cli.detach();

  co_return;
}

void tag_recv_cb(void *request, ucs_status_t, ucp_tag_recv_info_t const *,
                 void *) {
  ::ucp_request_release(request);
  gOutstandingRecv--;
  gCounter.fetch_add(1, std::memory_order_relaxed);
}

ucxpp::task<void> server(ucxpp::acceptor acceptor) {
  auto ep = co_await acceptor.accept();

  std::thread srv([ep]() {
    ep->print();
    void *buffer = nullptr;
    ucp_mem_h memh;
    ucs_status_t status;
    status =
        ucp_perf_mem_alloc(ep->worker_ptr()->context_ptr(), &buffer, &memh);
    if (status != UCS_OK) {
      std::cerr << "Failed to allocate memory: " << ucs_status_string(status)
                << std::endl;
      return;
    }

    ucp_request_param_t recv_param;
    recv_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK;
    recv_param.cb.recv = tag_recv_cb;
    auto worker_h = ep->worker_ptr()->handle();
    while (true) {
      while (gOutstandingRecv >= kConcurrency) {
        ::ucp_worker_progress(worker_h);
      }
      auto request = ::ucp_tag_recv_nbx(worker_h, buffer, gMsgSize, kTestTag,
                                        0xFFFFFFFFFFFFFFFF, &recv_param);
      if (UCS_PTR_IS_ERR(request)) {
        std::cout << "error: " << ucs_status_string(UCS_PTR_STATUS(request))
                  << std::endl;
      } else if (UCS_PTR_IS_PTR(request)) {
        gOutstandingRecv++;
      } else {
        gCounter.fetch_add(1, std::memory_order_seq_cst);
      }
    }
  });
  srv.detach();

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
    std::this_thread::yield();
  }
  stopped = true;
  loop->close();
  looper.join();
  reporter.join();
  return 0;
}
