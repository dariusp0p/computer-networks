// 4. Implement the Chat server example (see the link bellow) using UDP and TCP
// –only this time each client should contact the server just for registration.
// All communication happens directly between the peers (clients) without
// passing trough the server. Each client maintains an endpoint (TCP/UDP) with
// the server just for learning the arrival/departure of other clients. You
// create a mesh architecture where all clients connect directly between each
// others.

/*
 Protocol (over TCP):
  - Client -> Server: "REGISTER <ip:udp_port>\n"  or "REGISTER <udp_port>\n"
  - Server -> New client: multiple "PEER <ip:port>\n" followed by "END\n"
  - Server -> All other clients: "NEW <ip:port>\n" when someone registers
  - Server -> All clients: "LEFT <ip:port>\n" when someone disconnects
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXBUF 512
#define MAXPEERS FD_SETSIZE

typedef struct {
    int tcp_fd;
    char ip[INET_ADDRSTRLEN];
    int udp_port;
} peer_t;

static peer_t peers[MAXPEERS];
static fd_set master, read_fds;
static int fdmax = 0;
static int listener = -1;

static void remove_peer(int idx) {
    if (peers[idx].tcp_fd != -1) {
        close(peers[idx].tcp_fd);
        FD_CLR(peers[idx].tcp_fd, &master);
        peers[idx].tcp_fd = -1;
        peers[idx].ip[0] = '\0';
        peers[idx].udp_port = 0;
    }
}

static void broadcast_except(int except_fd, const char *msg, size_t len) {
    for (int i = 0; i <= fdmax; ++i) {
        for (int p = 0; p < MAXPEERS; ++p) {
            if (peers[p].tcp_fd == i && i != except_fd) {
                ssize_t s = send(i, msg, len, 0);
                if (s == -1) {
                    perror("send");
                    remove_peer(p);
                }
                break;
            }
        }
    }
}

static int find_free_slot(void) {
    for (int i = 0; i < MAXPEERS; ++i) {
        if (peers[i].tcp_fd == -1) return i;
    }
    return -1;
}

static void send_peer_list(int dest_fd) {
    char buf[MAXBUF];
    for (int i = 0; i < MAXPEERS; ++i) {
        if (peers[i].tcp_fd != -1) {
            int n = snprintf(buf, sizeof(buf), "PEER %s:%d\n", peers[i].ip, peers[i].udp_port);
            if (n > 0) send(dest_fd, buf, n, 0);
        }
    }
    send(dest_fd, "END\n", 4, 0);
}

static int parse_registration(const char *line, char *out_ip, size_t iplen, int *out_port, int sockfd) {
    const char *p = line;
    while (*p == ' ') ++p;
    if (strncasecmp(p, "REGISTER", 8) != 0) return -1;
    p += 8;
    while (*p == ' ') ++p;
    if (!*p) return -1;
    char tmp[MAXBUF];
    strncpy(tmp, p, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *nl = strchr(tmp, '\n');
    if (nl) *nl = '\0';
    char *colon = strchr(tmp, ':');
    if (colon) {
        *colon = '\0';
        strncpy(out_ip, tmp, iplen - 1);
        out_ip[iplen - 1] = '\0';
        *out_port = atoi(colon + 1);
    } else {
        struct sockaddr_in addr;
        socklen_t alen = sizeof(addr);
        if (getpeername(sockfd, (struct sockaddr *)&addr, &alen) < 0) return -1;
        if (inet_ntop(AF_INET, &addr.sin_addr, out_ip, iplen) == NULL) return -1;
        *out_port = atoi(tmp);
    }
    if (*out_port <= 0 || *out_port > 65535) return -1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <tcp_port>\n", argv[0]);
        exit(1);
    }

    int tcp_port = atoi(argv[1]);
    if (tcp_port <= 0 || tcp_port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        exit(1);
    }

    for (int i = 0; i < MAXPEERS; ++i) peers[i].tcp_fd = -1;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) {
        perror("socket");
        exit(1);
    }

    int yes = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
        close(listener);
        exit(1);
    }

    struct sockaddr_in myaddr;
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons((unsigned short)tcp_port);

    if (bind(listener, (struct sockaddr *)&myaddr, sizeof(myaddr)) == -1) {
        perror("bind");
        close(listener);
        exit(1);
    }

    if (listen(listener, 10) == -1) {
        perror("listen");
        close(listener);
        exit(1);
    }

    FD_SET(listener, &master);
    fdmax = listener;

    printf("Registration server listening on %d\n", tcp_port);

    while (1) {
        read_fds = master;
        int sel = select(fdmax + 1, &read_fds, NULL, NULL, NULL);
        if (sel == -1) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        for (int i = 0; i <= fdmax; ++i) {
            if (!FD_ISSET(i, &read_fds)) continue;

            if (i == listener) {
                /* new connection */
                struct sockaddr_in remote;
                socklen_t addrlen = sizeof(remote);
                int newfd = accept(listener, (struct sockaddr *)&remote, &addrlen);
                if (newfd == -1) {
                    perror("accept");
                    continue;
                }
                /* read registration line (simple blocking read) */
                char buf[MAXBUF];
                ssize_t r = recv(newfd, buf, sizeof(buf) - 1, 0);
                if (r <= 0) {
                    close(newfd);
                    continue;
                }
                buf[r] = '\0';
                char peer_ip[INET_ADDRSTRLEN];
                int peer_udp = 0;
                if (parse_registration(buf, peer_ip, sizeof(peer_ip), &peer_udp, newfd) < 0) {
                    const char *err = "ERROR Invalid REGISTER format\n";
                    send(newfd, err, strlen(err), 0);
                    close(newfd);
                    continue;
                }

                int slot = find_free_slot();
                if (slot < 0) {
                    const char *err = "ERROR Server full\n";
                    send(newfd, err, strlen(err), 0);
                    close(newfd);
                    continue;
                }

                peers[slot].tcp_fd = newfd;
                strncpy(peers[slot].ip, peer_ip, sizeof(peers[slot].ip) - 1);
                peers[slot].ip[sizeof(peers[slot].ip) - 1] = '\0';
                peers[slot].udp_port = peer_udp;

                FD_SET(newfd, &master);
                if (newfd > fdmax) fdmax = newfd;

                /* send existing peer list to new client */
                send_peer_list(newfd);

                /* notify others */
                char notify[MAXBUF];
                int n = snprintf(notify, sizeof(notify), "NEW %s:%d\n", peer_ip, peer_udp);
                broadcast_except(newfd, notify, (size_t)n);

                printf("Registered %s:%d (tcp fd %d)\n", peer_ip, peer_udp, newfd);

            } else {
                /* data from existing client or disconnect */
                char buf[MAXBUF];
                ssize_t nbytes = recv(i, buf, sizeof(buf) - 1, 0);
                if (nbytes <= 0) {
                    /* disconnect */
                    if (nbytes == 0) {
                        /* find which peer */
                        for (int p = 0; p < MAXPEERS; ++p) {
                            if (peers[p].tcp_fd == i) {
                                char notify[MAXBUF];
                                int len = snprintf(notify, sizeof(notify), "LEFT %s:%d\n", peers[p].ip, peers[p].udp_port);
                                broadcast_except(i, notify, (size_t)len);
                                printf("Peer %s:%d disconnected (fd %d)\n", peers[p].ip, peers[p].udp_port, i);
                                remove_peer(p);
                                break;
                            }
                        }
                    } else {
                        perror("recv");
                    }
                } else {
                    buf[nbytes] = '\0';
                    /* optionally handle in-band commands like QUIT */
                    if (strncasecmp(buf, "QUIT", 4) == 0) {
                        for (int p = 0; p < MAXPEERS; ++p) {
                            if (peers[p].tcp_fd == i) {
                                char notify[MAXBUF];
                                int len = snprintf(notify, sizeof(notify), "LEFT %s:%d\n", peers[p].ip, peers[p].udp_port);
                                broadcast_except(i, notify, (size_t)len);
                                printf("Peer %s:%d requested QUIT (fd %d)\n", peers[p].ip, peers[p].udp_port, i);
                                remove_peer(p);
                                break;
                            }
                        }
                    } else {
                        /* ignore other messages - server is only registry */
                        const char *ack = "OK\n";
                        send(i, ack, strlen(ack), 0);
                    }
                }
            }
        } /* for i..fdmax */
    }

    /* cleanup */
    for (int p = 0; p < MAXPEERS; ++p) if (peers[p].tcp_fd != -1) close(peers[p].tcp_fd);
    if (listener != -1) close(listener);
    return 0;
}