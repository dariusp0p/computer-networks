import socket
import argparse
import sys


def handle_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", "-H", required=True, help="Server IP")
    parser.add_argument("--port", "-p", type=int, required=True, help="Server tcp port")

    args = parser.parse_args()

    if not (1 <= args.port <= 65535):
        parser.error(f"Invalid port number: {args.port}!")
    return args


def print_menu():
    print("1. GET [service name]")
    print("2. SET [service name] [ip:port] [password]")
    print("0. EXIT")


def main():
    try:
        args = handle_arguments()

        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((args.host, args.port))

        while True:
            print_menu()
            choice = input("Enter your choice: ")

            if choice == "1":
                service_name = input("Enter service name: ")
                sock.send("G".encode("utf-8"))
                sock.send(service_name.encode("utf-8"))

                response = sock.recv(1024).decode("utf-8")
                if response == "NF":
                    print("Not found!")
                elif response == "TCP":
                    print("TCP Req!")
                else:
                    print(response)


            elif choice == "2":
                service_name = input("Enter service name: ")
                address = input("Enter IP address: ")
                port = input("Enter port: ")
                password = input("Enter password: ")
                sock.send("S".encode("utf-8"))

                sock.send(len(service_name).to_bytes(2, byteorder="big"))
                sock.send(service_name.encode("utf-8"))

                sock.send(len(address).to_bytes(2, byteorder="big"))
                sock.send(address.encode("utf-8"))

                sock.send(len(port).to_bytes(2, byteorder="big"))
                sock.send(port.encode("utf-8"))

                sock.send(len(password).to_bytes(2, byteorder="big"))
                sock.send(password.encode("utf-8"))

                response = sock.recv(2).decode("utf-8")
                print(response)
                if response == "OK":
                    print("Success!")
                elif response == "ER":
                    print("Wrong password!")

            elif choice == "0":
                sock.send("E".encode("utf-8"))
                break

            else:
                print("Invalid choice!")

        sock.close()
    except Exception as e:
        print(e)


if __name__ == '__main__':
    sys.exit(main())
