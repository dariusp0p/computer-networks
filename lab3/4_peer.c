// 4. Implement the Chat server example (see the link bellow) using UDP and TCP
// –only this time each client should contact the server just for registration.
// All communication happens directly between the peers (clients) without
// passing trough the server. Each client maintains an endpoint (TCP/UDP) with
// the server just for learning the arrival/departure of other clients. You
// create a mesh architecture where all clients connect directly between each
// others.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAXPEERS 128
#define LINEBUF 512
#define UDP_RECV_BUF 1024

typedef struct {
    struct sockaddr_in addr;
    int active;
} peer_t;

static peer_t peers[MAXPEERS];
static pthread_mutex_t peers_lock = PTHREAD_MUTEX_INITIALIZER;
static int udp_sock = -1;
static int tcp_sock = -1;
static volatile sig_atomic_t running = 1;
static pthread_t th_udp = 0;
static pthread_t th_tcp = 0;

static void add_peer(const char* ip, int port) {
    pthread_mutex_lock(&peers_lock);
    for (int i = 0; i < MAXPEERS; ++i) {
        if (peers[i].active) {
            if (peers[i].addr.sin_port == htons((unsigned short)port) &&
                peers[i].addr.sin_addr.s_addr == inet_addr(ip)) {
                pthread_mutex_unlock(&peers_lock);
                return;
            }
        } else {
            peers[i].active = 1;
            peers[i].addr.sin_family = AF_INET;
            peers[i].addr.sin_port = htons((unsigned short)port);
            inet_pton(AF_INET, ip, &peers[i].addr.sin_addr);
            pthread_mutex_unlock(&peers_lock);
            return;
        }
    }
    pthread_mutex_unlock(&peers_lock);
}

static void remove_peer(const char* ip, int port) {
    pthread_mutex_lock(&peers_lock);
    in_addr_t a = inet_addr(ip);
    for (int i = 0; i < MAXPEERS; ++i) {
        if (peers[i].active &&
            peers[i].addr.sin_port == htons((unsigned short)port) &&
            peers[i].addr.sin_addr.s_addr == a) {
            peers[i].active = 0;
            break;
        }
    }
    pthread_mutex_unlock(&peers_lock);
}

static void list_peers() {
    char tmp[INET_ADDRSTRLEN];
    pthread_mutex_lock(&peers_lock);
    printf("Peers:\n");
    for (int i = 0; i < MAXPEERS; ++i) {
        if (peers[i].active) {
            inet_ntop(AF_INET, &peers[i].addr.sin_addr, tmp, sizeof(tmp));
            printf(" [%d] %s:%d\n", i, tmp, ntohs(peers[i].addr.sin_port));
        }
    }
    pthread_mutex_unlock(&peers_lock);
}

static ssize_t send_udp_to_peer(const struct sockaddr_in *peer, const char* msg, int len) {
    return sendto(udp_sock, msg, len, 0, (const struct sockaddr*)peer, sizeof(*peer));
}

static void broadcast_udp(const char* msg, int len) {
    pthread_mutex_lock(&peers_lock);
    for (int i = 0; i < MAXPEERS; ++i) {
        if (peers[i].active) send_udp_to_peer(&peers[i].addr, msg, len);
    }
    pthread_mutex_unlock(&peers_lock);
}

static void* udp_recv_thread(void *arg) {
    (void)arg;
    char buf[UDP_RECV_BUF];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    while (running) {
        ssize_t n = recvfrom(udp_sock, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&from, &fromlen);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (!running) break;
            perror("udp recvfrom");
            break;
        }
        if (n > 0) {
            buf[n] = '\0';
            char saddr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from.sin_addr, saddr, sizeof(saddr));
            printf("\n[UDP %s:%d] %s\n> ", saddr, ntohs(from.sin_port), buf);
            fflush(stdout);
        }
    }
    return NULL;
}

static void handle_server_line(const char* line) {
    // "PEER ip:port", "NEW ip:port", "LEFT ip:port"
    if (strncmp(line, "PEER ", 5) == 0 || strncmp(line, "NEW ", 4) == 0) {
        const char *p = strchr(line, ' ');
        if (!p) return;
        ++p;
        char ip[INET_ADDRSTRLEN];
        int port = 0;
        const char *c = strchr(p, ':');
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
        char ip[INET_ADDRSTRLEN];
        int port = 0;
        const char *c = strchr(p, ':');
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

static void* tcp_reader_thread(void *arg) {
    (void)arg;
    char buf[LINEBUF];
    char linebuf[LINEBUF];
    int linepos = 0;
    while (running) {
        ssize_t n = recv(tcp_sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            if (n == 0) {
                fprintf(stderr, "server closed connection\n");
            } else {
                perror("tcp recv");
            }
            running = 0;
            break;
        }
        for (ssize_t i = 0; i < n; ++i) {
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
    return NULL;
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

    // create and bind UDP socket
    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) { perror("udp socket"); return 1; }

    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons((unsigned short)udp_port);

    if (bind(udp_sock, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) {
        perror("bind udp");
        close(udp_sock);
        return 1;
    }

    // create and connect TCP socket to server
    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) { perror("tcp socket"); close(udp_sock); return 1; }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons((unsigned short)server_port);
    if (inet_pton(AF_INET, server_ip, &srv.sin_addr) != 1) {
        fprintf(stderr, "invalid server ip\n");
        close(tcp_sock); close(udp_sock);
        return 1;
    }

    if (connect(tcp_sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("connect");
        close(tcp_sock); close(udp_sock);
        return 1;
    }

    // determine local IP after TCP connect
    struct sockaddr_in loc;
    socklen_t loclen = sizeof(loc);
    if (getsockname(tcp_sock, (struct sockaddr*)&loc, &loclen) < 0) {
        perror("getsockname");
    }
    char local_ip[INET_ADDRSTRLEN] = "127.0.0.1";
    inet_ntop(AF_INET, &loc.sin_addr, local_ip, sizeof(local_ip));

    // get actual UDP bound port
    struct sockaddr_in udp_bound;
    socklen_t udp_bound_len = sizeof(udp_bound);
    if (getsockname(udp_sock, (struct sockaddr*)&udp_bound, &udp_bound_len) < 0) {
        perror("udp getsockname");
    }
    int bound_udp_port = ntohs(udp_bound.sin_port);

    // send REGISTER <ip:port>\n
    char reg[128];
    int reglen = snprintf(reg, sizeof(reg), "REGISTER %s:%d\n", local_ip, bound_udp_port);
    if (send(tcp_sock, reg, reglen, 0) < 0) perror("send register");

    // start threads
    if (pthread_create(&th_udp, NULL, udp_recv_thread, NULL) != 0) {
        perror("pthread_create udp");
        running = 0;
    }
    if (pthread_create(&th_tcp, NULL, tcp_reader_thread, NULL) != 0) {
        perror("pthread_create tcp");
        running = 0;
    }

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
                /* move p past the index, then skip spaces to the message start */
                char *sp = p;
                while (*sp && !isspace((unsigned char)*sp)) ++sp; /* skip the index token */
                while (*sp && isspace((unsigned char)*sp)) ++sp;  /* skip spaces */
                /* strip trailing newline */
                char *nl = strchr(sp, '\n');
                if (nl) *nl = '\0';
                if (idx >= 0 && idx < MAXPEERS && *sp) {
                    struct sockaddr_in dst;
                    int active = 0;
                    pthread_mutex_lock(&peers_lock);
                    if (peers[idx].active) {
                        dst = peers[idx].addr; /* copy under lock */
                        active = 1;
                    }
                    pthread_mutex_unlock(&peers_lock);

                    if (!active) {
                        printf("invalid index or message\n");
                    } else {
                        char saddr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &dst.sin_addr, saddr, sizeof(saddr));
                        ssize_t sent = send_udp_to_peer(&dst, sp, (int)strlen(sp));
                        if (sent < 0) {
                            perror("sendto");
                        } else {
                            printf("-> sent to %s:%d (%zd bytes)\n", saddr, ntohs(dst.sin_port), sent);
                        }
                    }
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
            send(tcp_sock, "QUIT\n", 5, 0);
            running = 0;
            break;
        } else if (line[0] == '\n') {
            continue;
        } else {
            broadcast_udp(line, (int)strlen(line));
        }
    }

    // cleanup
    running = 0;

    if (tcp_sock >= 0) {
        shutdown(tcp_sock, SHUT_RDWR);
    }

    if (udp_sock >= 0) {
        int wake = socket(AF_INET, SOCK_DGRAM, 0);
        if (wake >= 0) {
            sendto(wake, "x", 1, 0, (struct sockaddr*)&udp_bound, sizeof(udp_bound));
            close(wake);
        }
    }

    if (th_udp) pthread_join(th_udp, NULL);
    if (th_tcp) pthread_join(th_tcp, NULL);

    if (tcp_sock >= 0) { close(tcp_sock); tcp_sock = -1; }
    if (udp_sock >= 0) { close(udp_sock); udp_sock = -1; }

    pthread_mutex_destroy(&peers_lock);
    printf("client exited\n");
    return 0;
}