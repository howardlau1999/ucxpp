# UCX++

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

[Documentation](https://liuhaohua.com/ucxpp/) | [Examples](https://liuhaohua.com/ucxpp/examples.html)

This library relieves your pain of writing asynchronous UCX code.

## Quick Example

Check [helloworld.cc](./examples/helloworld.cc) for API usage example.

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
