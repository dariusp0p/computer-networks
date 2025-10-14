// 12. The client sends a date string in YYYY-MM-DD format. The server returns the day of the week corresponding to that date.

#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <stdio.h>
#include <string.h>

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


const char* get_day_of_week(const char* date_str) {
    int y, m, d;
    if (sscanf(date_str, "%d-%d-%d", &y, &m, &d) != 3)
        return "Invalid date";

    if (m < 3) {
        m += 12;
        y -= 1;
    }
    int K = y % 100;
    int J = y / 100;
    int h = (d + 13*(m+1)/5 + K + K/4 + J/4 + 5*J) % 7;

    const char* days[] = {"Saturday", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday"};
    return days[h];
}


int main() {
    SOCKET sock;
    struct sockaddr_in server, client;
    int len = sizeof(client);
    int cl, err;

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) < 0) {
        printf("Error initializing the Windows Sockets Library!\n");
        return 1;
    }
#endif

    sock = socket(AF_INET, SOCK_DGRAM, 0); // modified here
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

    while (1) {
        char buffer[1024];

        int res = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&client, &len);
        if (res < 0) {
            perror("Error receiving!\n");
            continue;
        }
        buffer[res] = '\0';

        const char* day = get_day_of_week(buffer);
        sendto(sock, day, strlen(day) + 1, 0, (struct sockaddr*)&client, len);
    }

    closesocket(sock);

#ifdef _WIN32
    WSACleanup();
#endif
}
