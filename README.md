# port-scanner-cpp ⚡

Multithreaded TCP port scanner in pure C++20. No external dependencies.

## Features
- Multithreaded with configurable thread count
- Non-blocking connect with timeout
- Cross-platform (Linux, macOS, Windows)
- DNS resolution support

## Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/port_scanner 192.168.1.1 1 65535 500 500
```

## Usage
```
port_scanner <target> [start=1] [end=1024] [threads=200] [timeout_ms=1000]
```

## License
MIT
