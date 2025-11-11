import socket
import threading


data_lock = threading.Lock()

server_password = "admin123"
data = {
    "web-server": [("192.168.1.10", "8080"), ("192.168.1.11", "8080")],
    "db-server": [("192.168.1.20", "5432")],
    "api-gateway": [("192.168.1.30", "3000"), ("192.168.1.31", "3000")]
}

def make_response(service_name):
    response = ""
    if service_name in data:
        for ip_port in data[service_name]:
            response += f"{ip_port[0]}:{ip_port[1]}\n"
    return response


def handle_client(client, addr):
    global data, server_password

    try:
        while True:
            request = client.recv(1).decode()

            if request == 'G':
                print(f"GET request from {addr}")
                service_name = client.recv(1024).decode()

                if service_name in data:
                    response = make_response(service_name)
                    client.sendall(response.encode())
                else:
                    print(f"{service_name} not found!")
                    client.send("NF".encode())

            elif request == 'S':
                print(f"SET request from {addr}")

                length = int.from_bytes(client.recv(2), byteorder="big")
                service_name = client.recv(length).decode()

                length = int.from_bytes(client.recv(2), byteorder="big")
                address = client.recv(length).decode()

                length = int.from_bytes(client.recv(2), byteorder="big")
                port = client.recv(length).decode()

                length = int.from_bytes(client.recv(2), byteorder="big")
                password = client.recv(length).decode()

                if password == server_password:
                    data_lock.acquire()
                    if service_name in data:
                        data[service_name].append((address, port))
                    else:
                        data[service_name] = [(address, port)]
                    data_lock.release()
                    print(f"{service_name} set successfully!")
                    client.send("OK".encode())
                else:
                    client.send("ER".encode())

            elif request == 'E':
                print(f"Exit request from {addr}")
                break

            else:
                print("Invalid request")

            client.close()
    except Exception as e:
        print(e)


def main():
    try:
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("0.0.0.0", 9999))
        server.listen(5)

        print("Listening for connections...")
        while True:
            client, addr = server.accept()
            print("Client connected from", addr)
            thread = threading.Thread(target=handle_client, args=(client, addr), daemon=True)
            thread.start()
    except Exception as e:
        print(e)


if __name__ == '__main__':
    main()
