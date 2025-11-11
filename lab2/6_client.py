# The server chooses a random integer number. Each client generates a random integer number and send it to the server.
# The server answers with the message “larger” if the client has sent a smaller number than the server’s choice, or
# with message “smaller” if the client has send a larger number than the server’s choice. Each client continues
# generating a different random number (larger or smaller than the previous) according to the server’s indication. When
# a client guesses the server choice – the server sends back to the winner the message “You win – within x tries”.
# It also sends back to all other clients the message “You lost – after y retries!” (x and y are the number of tries of
# each respective client). The server closes all connections upon a win and it chooses a different random integer for
# the next game (set of clients)

import socket
import random
import argparse
import sys
import time


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

    min, max = 1, 1000
    guess = random.randint(min, max)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((args.host, args.port))
    print(f"Connected to {args.host}:{args.port}")

    game_on = True
    while game_on:
        sock.send(guess.to_bytes(4, byteorder="big"))
        print(f"Sent {guess} to server.")
        response = sock.recv(1024).decode("utf-8")
        if response == "You won!":
            print(f"You won! The number was {guess}")
            game_on = False
        elif response == "Smaller!":
            print("Smaller!")
            min = guess + 1
            guess = random.randint(min, max)
        elif response == "Larger!":
            print("Larger!")
            max = guess - 1
            guess = random.randint(min, max)
        time.sleep(3)

if __name__ == "__main__":
    sys.exit(main())