# UCX++

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

[Documentation](https://liuhaohua.com/ucxpp/) | [Examples](https://liuhaohua.com/ucxpp/examples.html)

This library relieves your pain of writing asynchronous UCX code.

## Quick Example

Create a UCX context, an event loop for initial address exchange, a UCX worker, and threads to poll the event loop and make the worker progress:

```cpp

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
  // ...
}
```

On the server-side, create an endpoint acceptor:

```cpp
ucxpp::task<void> server(ucxpp::acceptor &acceptor) {
  auto ep = co_await acceptor.accept();
  // operation on ep...
}

int main(int argc, char *argv[]) {
  // ...
  auto listener = std::make_shared<ucxpp::socket::tcp_listener>(
        loop, "", std::stoi(argv[1]));
  auto acceptor = ucxpp::acceptor(worker, listener);
  server(acceptor);
}
```

On the client-side, create an endpoint connector:

```cpp
ucxpp::task<void> client(ucxpp::connector &connector) {
  auto ep = co_await connector.connect();
  // operation on ep...
}

int main(int argc, char *argv[]) {
  // ...
  auto connector =
        ucxpp::connector(worker, loop, argv[1], std::stoi(argv[2]));
  client(connector);
}
```

## Building

Requires: C++ compiler with C++20 standard support and `ucx` development headers installed.

```bash
git clone https://github.com/howardlau1999/ucxpp && cd ucxpp
cmake -Bbuild -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR .
cmake --build build

# To install
cmake --install build
```

## Developing

Install `clang-format` and `pre-commit`. 

```bash
pip install pre-commit
pre-commit install
```