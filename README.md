# Cisco-Style Multi-Client TCP Echo Server (BSD Sockets)

A small, networking project that demonstrates the core building blocks of data-path / systems software:
**TCP sockets, multi-client I/O with `poll()`, line-based protocols, partial read/write handling, and basic testing.**

This is intentionally minimal, fast to review, and aligned with typical networking/software roles.

---

## Features

- **Multi-client TCP server** (single process) using **`poll()`**
- **Line-based protocol**: messages end with `\n`
- Handles **partial reads/writes** with per-client buffers
- Simple **CLI client** to connect, send lines, and print echoed responses
- **Automated test** that launches multiple clients and verifies echo behavior
- Small repo, clear structure, easy to extend (chat/broadcast mode, TLS, telemetry, etc.)

---

## Project Structure


```
.
├── include/
│   └── common.hpp
├── src/
│   ├── server.cpp
│   └── client.cpp
├── tests/
│   └── test_echo.py
├── scripts/
│   └── run_local.sh
├── Makefile
└── README.md


```

````

---

## Build

### Linux / WSL

```bash
make clean
make
````

Produces:

* `./server`
* `./client`

---

## Run

### Terminal 1 — start the server

```bash
./server 9090
```

Expected output:

```
Echo server listening on 0.0.0.0:9090
Protocol: send lines ending with \n; server echoes each line.
```

### Terminal 2 — run the client

```bash
./client 127.0.0.1 9090
```

Type lines and press Enter:

```
hello
```

Client prints:

```
echo: hello
```

Exit: `Ctrl+D` (EOF)

---

## Testing

The Python test spawns multiple clients and validates echoed responses.

> If you already have a server running on the same port, stop it first.

```bash
python3 tests/test_echo.py
```

Expected:

```
PASS: echo server works for multiple clients
```

---

## Debug / Diagnostics (quick commands)

These are common tools used when debugging networking/system services.

### gdb

```bash
gdb --args ./server 9090
run
bt
```

### valgrind (memory checks)

```bash
valgrind --leak-check=full ./server 9090
```

### strace (syscall tracing)

```bash
strace -f -o trace.log ./server 9090
```

---

## Why this project matters

This repo demonstrates:

* Systems programming fundamentals in **C/C++**
* Network programming with **BSD sockets**
* Event-loop concurrency using **`poll()`**
* Robustness fundamentals (buffering, partial I/O, clean error handling)
* Basic automated testing and reproducible build steps

---

## Possible Extensions (future)

* Chat-room / broadcast mode (send one client’s line to all)
* gRPC/Telemetry endpoint (expose metrics like client count, bytes read/written)
* TLS support (OpenSSL)
* CI pipeline (GitHub Actions build + test)
* Packet parsing demo (Ethernet/IP/TCP header decoding)

---