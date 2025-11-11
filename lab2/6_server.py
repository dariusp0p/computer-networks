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
import threading


winning_lock = threading.Lock()
someone_guessed = False
winner_client = 0


def handle_client(conn, addr, answer):
    global winning_lock
    nr_of_tries = 0
    while True:
        guess = int.from_bytes(conn.recv(4), "big")
        print(f"Client {addr} tries: {guess}")
        nr_of_tries += 1
        if guess == answer:
            conn.send("You won!".encode("utf-8"))
            winning_lock.acquire()
            someone_guessed = True
            winner_client = threading.get_ident()
            winning_lock.release()

        elif guess > answer:
            conn.send("Larger!".encode("utf-8"))
        elif guess < answer:
            conn.send("Smaller!".encode("utf-8"))

    conn.close()


def main():
    answer = random.randint(1, 1000)
    print(f"The answer is {answer}.")

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("0.0.0.0", 12345))
    sock.listen(10)

    print("Waiting for connection...")
    while True:
        conn, addr = sock.accept()
        print(f"Client {addr} connected.")
        threading.Thread(target=handle_client, args=(conn, addr, answer), daemon=True).start()


if __name__ == "__main__":
    main()
