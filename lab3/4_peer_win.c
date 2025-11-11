// c
// File: `lab3/4_client_windows.c`
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

// 4. Implement the Chat server example (see the link bellow) using UDP and TCP
// –only this time each client should contact the server just for registration.
// All communication happens directly between the peers (clients) without
// passing trough the server. Each client maintains an endpoint (TCP/UDP) with
// the server just for learning the arrival/departure of other clients. You
// create a mesh architecture where all clients connect directly between each
// others.

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#pragma comment(lib, "Ws2_32.lib")

#define MAXPEERS 128
#define LINEBUF 512
#define UDP_RECV_BUF 1024

typedef struct {
    struct sockaddr_in addr;
    int active;
} peer_t;

static peer_t peers[MAXPEERS];
static CRITICAL_SECTION peers_lock;
static SOCKET udp_sock = INVALID_SOCKET;
static SOCKET tcp_sock = INVALID_SOCKET;
static volatile int running = 1;
static HANDLE th_udp = NULL;
static HANDLE th_tcp = NULL;

static void add_peer(const char* ip, int port) {
    EnterCriticalSection(&peers_lock);
    for (int i = 0; i < MAXPEERS; ++i) {
        if (peers[i].active) {
            if (peers[i].addr.sin_port == htons((unsigned short)port) &&
                strcmp(inet_ntoa(peers[i].addr.sin_addr), ip) == 0) {
                LeaveCriticalSection(&peers_lock);
                return; // already present
            }
        } else {
            peers[i].active = 1;
            peers[i].addr.sin_family = AF_INET;
            peers[i].addr.sin_port = htons((unsigned short)port);
            inet_pton(AF_INET, ip, &peers[i].addr.sin_addr);
            LeaveCriticalSection(&peers_lock);
            return;
        }
    }
    LeaveCriticalSection(&peers_lock);
}

static void remove_peer(const char* ip, int port) {
    EnterCriticalSection(&peers_lock);
    for (int i = 0; i < MAXPEERS; ++i) {
        if (peers[i].active &&
            peers[i].addr.sin_port == htons((unsigned short)port) &&
            strcmp(inet_ntoa(peers[i].addr.sin_addr), ip) == 0) {
            peers[i].active = 0;
            break;
        }
    }
    LeaveCriticalSection(&peers_lock);
}

static void list_peers() {
    EnterCriticalSection(&peers_lock);
    printf("Peers:\n");
    for (int i = 0; i < MAXPEERS; ++i) {
        if (peers[i].active) {
            printf(" [%d] %s:%d\n", i, inet_ntoa(peers[i].addr.sin_addr), ntohs(peers[i].addr.sin_port));
        }
    }
    LeaveCriticalSection(&peers_lock);
}

static void send_udp_to_peer(const struct sockaddr_in *peer, const char* msg, int len) {
    sendto(udp_sock, msg, len, 0, (struct sockaddr*)peer, sizeof(*peer));
}

static void broadcast_udp(const char* msg, int len) {
    EnterCriticalSection(&peers_lock);
    for (int i = 0; i < MAXPEERS; ++i) {
        if (peers[i].active) send_udp_to_peer(&peers[i].addr, msg, len);
    }
    LeaveCriticalSection(&peers_lock);
}

DWORD WINAPI udp_recv_thread(LPVOID arg) {
    (void)arg;
    char buf[UDP_RECV_BUF];
    struct sockaddr_in from;
    int fromlen = sizeof(from);
    while (running) {
        int n = recvfrom(udp_sock, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&from, &fromlen);
        if (n == SOCKET_ERROR) {
            if (!running) break;
            int e = WSAGetLastError();
            if (e == WSAEINTR || e == WSAEWOULDBLOCK) continue;
            fprintf(stderr, "udp recvfrom error: %d\n", e);
            break;
        }
        if (n > 0) {
            buf[n] = '\0';
            printf("\n[UDP %s:%d] %s\n> ", inet_ntoa(from.sin_addr), ntohs(from.sin_port), buf);
            fflush(stdout);
        }
    }
    return 0;
}

static void handle_server_line(const char* line) {
    // lines: "PEER ip:port", "NEW ip:port", "LEFT ip:port"
    if (strncmp(line, "PEER ", 5) == 0 || strncmp(line, "NEW ", 4) == 0) {
        const char *p = strchr(line, ' ');
        if (!p) return;
        ++p;
        char ip[INET_ADDRSTRLEN]; int port = 0;
        char *c = strchr((char*)p, ':');
        if (c) {
            size_t iplen = c - p;
            if (iplen >= sizeof(ip)) iplen = sizeof(ip) - 1;
            memcpy(ip, p, iplen);
            ip[iplen] = '\0';
            port = atoi(c+1);
            add_peer(ip, port);
            printf("[server] %s\n", line);
        }
    } else if (strncmp(line, "LEFT ", 5) == 0) {
        const char *p = strchr(line, ' ');
        if (!p) return;
        ++p;
        char ip[INET_ADDRSTRLEN]; int port = 0;
        char *c = strchr((char*)p, ':');
        if (c) {
            size_t iplen = c - p;
            if (iplen >= sizeof(ip)) iplen = sizeof(ip) - 1;
            memcpy(ip, p, iplen);
            ip[iplen] = '\0';
            port = atoi(c+1);
            remove_peer(ip, port);
            printf("[server] %s\n", line);
        }
    } else {
        printf("[server] %s\n", line);
    }
}

DWORD WINAPI tcp_reader_thread(LPVOID arg) {
    (void)arg;
    char buf[LINEBUF];
    char linebuf[LINEBUF];
    int linepos = 0;
    while (running) {
        int n = recv(tcp_sock, buf, sizeof(buf) - 1, 0);
        if (n == 0 || n == SOCKET_ERROR) {
            if (n == SOCKET_ERROR) {
                int e = WSAGetLastError();
                if (e == WSAECONNRESET || e == WSAENOTCONN) {
                    fprintf(stderr, "server connection closed\n");
                } else {
                    fprintf(stderr, "tcp recv error: %d\n", e);
                }
            } else {
                fprintf(stderr, "server closed connection\n");
            }
            running = 0;
            break;
        }
        for (int i = 0; i < n; ++i) {
            char ch = buf[i];
            if (ch == '\r') continue;
            linebuf[linepos++] = ch;
            if (linepos >= (int)sizeof(linebuf)-1) linepos = (int)sizeof(linebuf)-2;
            if (ch == '\n') {
                linebuf[linepos] = '\0';
                handle_server_line(linebuf);
                linepos = 0;
            }
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <server_ip> <server_tcp_port> [udp_port]\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int udp_port = (argc == 4) ? atoi(argv[3]) : 0;
    if (server_port <= 0 || server_port > 65535) { fprintf(stderr, "bad server port\n"); return 1; }
    if (udp_port < 0 || udp_port > 65535) { fprintf(stderr, "bad udp port\n"); return 1; }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    InitializeCriticalSection(&peers_lock);
    for (int i = 0; i < MAXPEERS; ++i) peers[i].active = 0;

    // create and bind UDP socket
    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock == INVALID_SOCKET) { fprintf(stderr, "udp socket error: %d\n", WSAGetLastError()); return 1; }

    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons((unsigned short)udp_port);

    if (bind(udp_sock, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind udp failed: %d\n", WSAGetLastError());
        closesocket(udp_sock);
        return 1;
    }

    // create and connect TCP socket to server
    tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_sock == INVALID_SOCKET) { fprintf(stderr, "tcp socket error: %d\n", WSAGetLastError()); return 1; }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons((unsigned short)server_port);
    if (inet_pton(AF_INET, server_ip, &srv.sin_addr) != 1) {
        srv.sin_addr.s_addr = inet_addr(server_ip);
    }

    if (connect(tcp_sock, (struct sockaddr*)&srv, sizeof(srv)) == SOCKET_ERROR) {
        fprintf(stderr, "connect failed: %d\n", WSAGetLastError());
        closesocket(tcp_sock);
        closesocket(udp_sock);
        return 1;
    }

    // determine local IP after TCP connect
    struct sockaddr_in loc;
    int loclen = sizeof(loc);
    if (getsockname(tcp_sock, (struct sockaddr*)&loc, &loclen) == SOCKET_ERROR) {
        fprintf(stderr, "getsockname failed: %d\n", WSAGetLastError());
    }
    char local_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &loc.sin_addr, local_ip, sizeof(local_ip));

    // get actual UDP bound port (if 0 was requested)
    struct sockaddr_in udp_bound;
    int udp_bound_len = sizeof(udp_bound);
    if (getsockname(udp_sock, (struct sockaddr*)&udp_bound, &udp_bound_len) == SOCKET_ERROR) {
        fprintf(stderr, "udp getsockname failed: %d\n", WSAGetLastError());
    }
    int bound_udp_port = ntohs(udp_bound.sin_port);

    // send REGISTER <ip:port>\n
    char reg[128];
    int reglen = snprintf(reg, sizeof(reg), "REGISTER %s:%d\n", local_ip, bound_udp_port);
    send(tcp_sock, reg, reglen, 0);

    // start UDP recv thread
    th_udp = CreateThread(NULL, 0, udp_recv_thread, NULL, 0, NULL);
    // start TCP reader thread
    th_tcp = CreateThread(NULL, 0, tcp_reader_thread, NULL, 0, NULL);

    printf("Registered as %s:%d\n", local_ip, bound_udp_port);
    printf("Commands: /peers  /msg <index> <text>  /all <text>  /quit\n");

    char line[1024];
    while (running && fgets(line, sizeof(line), stdin) != NULL) {
        if (strncmp(line, "/peers", 6) == 0) {
            list_peers();
        } else if (strncmp(line, "/msg ", 5) == 0) {
            int idx = -1;
            char *p = line + 5;
            if (sscanf(p, "%d", &idx) == 1) {
                // find next space
                char *msg = strchr(p, ' ');
                if (msg) {
                    // advance to message text
                    msg = strchr(msg+1, ' ');
                    if (msg) ++msg;
                } else msg = NULL;
                if (idx >= 0 && idx < MAXPEERS && peers[idx].active && msg) {
                    send_udp_to_peer(&peers[idx].addr, msg, (int)strlen(msg));
                } else {
                    printf("invalid index or message\n");
                }
            } else {
                printf("usage: /msg <index> <text>\n");
            }
        } else if (strncmp(line, "/all ", 5) == 0) {
            char *msg = line + 5;
            broadcast_udp(msg, (int)strlen(msg));
        } else if (strncmp(line, "/quit", 5) == 0) {
            // notify server and exit
            send(tcp_sock, "QUIT\n", 5, 0);
            running = 0;
            break;
        } else if (line[0] == '\n') {
            continue;
        } else {
            // default: broadcast typed line
            broadcast_udp(line, (int)strlen(line));
        }
    }

    // cleanup
    running = 0;
    // close sockets to interrupt threads
    shutdown(tcp_sock, SD_BOTH);
    closesocket(tcp_sock);
    shutdown(udp_sock, SD_BOTH);
    closesocket(udp_sock);

    if (th_udp) WaitForSingleObject(th_udp, 2000);
    if (th_tcp) WaitForSingleObject(th_tcp, 2000);
    if (th_udp) CloseHandle(th_udp);
    if (th_tcp) CloseHandle(th_tcp);

    DeleteCriticalSection(&peers_lock);
    WSACleanup();
    printf("client exited\n");
    return 0;
}