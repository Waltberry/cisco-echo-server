import socket
import subprocess
import time
import os
import signal
import sys

HOST = "127.0.0.1"
PORT = 9090

def recv_line(sock: socket.socket) -> bytes:
    out = bytearray()
    while True:
        b = sock.recv(1)
        if not b:
            raise RuntimeError("server closed connection")
        out += b
        if b == b"\n":
            return bytes(out)

def main():
    # Start server
    server = subprocess.Popen(["./server", str(PORT)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        time.sleep(0.3)

        clients = []
        for _ in range(5):
            s = socket.create_connection((HOST, PORT), timeout=2)
            clients.append(s)

        msgs = [b"hello\n", b"world\n", b"cisco\n", b"data-path\n", b"testing\n"]
        for i, s in enumerate(clients):
            s.sendall(msgs[i])
        for i, s in enumerate(clients):
            echoed = recv_line(s)
            assert echoed == msgs[i], (echoed, msgs[i])

        # multi-line test on one client
        s = clients[0]
        s.sendall(b"a\nb\nc\n")
        assert recv_line(s) == b"a\n"
        assert recv_line(s) == b"b\n"
        assert recv_line(s) == b"c\n"

        for s in clients:
            s.close()

        print("PASS: echo server works for multiple clients")

    finally:
        # terminate server
        if server.poll() is None:
            server.send_signal(signal.SIGINT)
            try:
                server.wait(timeout=1)
            except subprocess.TimeoutExpired:
                server.kill()

if __name__ == "__main__":
    main()
