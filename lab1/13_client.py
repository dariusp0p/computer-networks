# 13.The client sends a small text file to the server. The server saves the file and returns the length of the received
# file content as an unsigned integer

import argparse
import os
import sys
import socket
from pathlib import Path


FILE_PATH = Path(__file__).resolve().parent / "data" / "13_ex.txt"


def handle_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", "-H", required=True, help="Server IP")
    parser.add_argument("--port", "-p", type=int, required=True, help="Server port")
    args = parser.parse_args()

    if not (1 <= args.port <= 65535):
        parser.error(f"Invalid port number: {args.port}!")
    return args


def main():
    args = handle_arguments()
    if not os.path.isfile(FILE_PATH):
        print(f"File {FILE_PATH} does not exist!")
        return 1

    data = open(FILE_PATH, "rb").read()
    header = len(data).to_bytes(4, "big") # 4\-byte unsigned big-endian length
    test = int()

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((args.host, args.port))
            print(f"Connected to server {args.host}:{args.port}.")

            sock.send(header)
            sock.sendall(data)

            response = sock.recv(4)
            print(f"Server reports received {int.from_bytes(response, "big")} bytes.")

        return 0

    except Exception as e:
        print(f"Failed to connect to server {args.host}:{args.port}: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
