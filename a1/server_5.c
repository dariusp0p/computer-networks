// 5. The client sends to the server an integer. The server returns the list of divisors for the specified number.

#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/ip.h>
    #include <arpa/inet.h>

    #include <unistd.h>
    #include <errno.h>

    #define closesocket close
    typedef int SOCKET;
#else
    #include <WinSock2.h>
    #include <stdint.h>
#endif


typedef struct Node {
  int value;
  struct Node* next;
} Node;

void append(Node** head, int value) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    new_node->value = value;
    new_node->next = NULL;
    if (*head == NULL) {
        *head = new_node;
    } else {
        Node* temp = *head;
        while (temp->next) temp = temp->next;
        temp->next = new_node;
    }
}

void free_list(Node* head) {
    while (head) {
        Node* temp = head;
        head = head->next;
        free(temp);
    }
}


int main() {
    SOCKET sock;
    struct sockaddr_in server, client;
    int cl, len, err;

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) < 0) {
        printf("Error initializing the Windows Sockets Library!\n");
        return 1;
    }
#endif

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Error creating server socket!\n");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_port = htons(1234);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("Error binding!\n");
        return 1;
    }

    listen(sock, 5);
    len = sizeof(client);
    memset(&client, 0, sizeof(client));

    while (1) {
        int buffer;

        printf("Listening for incomming connections...\n");
        cl = accept(sock, (struct sockaddr *) &client, &len);
        err = errno;
#ifdef _WIN32
        err = WSAGetLastError();
#endif
        if (cl < 0) {
            printf("Error accept: %d\n", err);
            continue;
        }
        printf("Connected client: %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        int res = recv(cl, (char*)&buffer, sizeof(buffer), 0);
        if (res != sizeof(buffer)) printf("Error receiving integer!\n");

        buffer = ntohl(buffer);

        Node* head = NULL;

        for (int i = 1; i <= buffer; i++) {
            if (buffer % i == 0) append(&head, i);
        }

        int count = 0;
        Node* temp = head;
        while (temp) {
            count++;
            temp = temp->next;
        }

        int net_count = htonl(count);
        send(cl, (char*)&net_count, sizeof(net_count), 0);

        temp = head;
        while (temp) {
            int net_div = htonl(temp->value);
            send(cl, (char*)&net_div, sizeof(net_div), 0);
            temp = temp->next;
        }

        free_list(head);

        closesocket(cl);
    }
#ifdef _WIN32
    WSACleanup();
#endif
}
