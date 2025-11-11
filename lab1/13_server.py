# 13.The client sends a small text file to the server. The server saves the file and returns the length of the received
# file content as an unsigned integer

import socket
import time
from pathlib import Path


ADDRESS = "0.0.0.0"
PORT = 12345


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((ADDRESS, PORT))
    sock.listen(5)

    print("Waiting for connection...")
    while True:
        conn, addr = sock.accept()
        print(f"Client {addr} connected on socket {conn}.")

        try:
            header = conn.recv(4)
            file_len = int.from_bytes(header, byteorder="big")

            filename = (Path(__file__).resolve().parent / "data" / f"received_{addr[0]}_{addr[1]}_{int(time.time())}.txt")
            filename.parent.mkdir(parents=True, exist_ok=True)

            remaining = file_len
            with open(filename, "wb") as f:
                while remaining:
                    chunk = conn.recv(min(4096, remaining))
                    if not chunk:
                        raise Exception("Connection closed while receiving payload!")
                    f.write(chunk)
                    remaining -= len(chunk)

            print(f"Saved {file_len} bytes to {filename}.")
            conn.send(file_len.to_bytes(4, "big"))

        except Exception as e:
            print(f"Connection {addr} error: {e}")


if __name__ == "__main__":
    main()
