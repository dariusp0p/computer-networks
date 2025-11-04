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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9034

fd_set master;
fd_set read_fds;
struct sockaddr_in myaddr;
struct sockaddr_in remoteaddr;
int fdmax;
int listener;
int newfd;
char buf[256], tmpbuf[256];
int nbytes, ret;
int yes = 1;
int addrlen;
int i, j, crt, int_port, client_count = 0;


struct sockaddr_in get_socket_name(int s, int local_or_remote) {
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    int ret;

    memset(&addr, 0, sizeof(addr));
    ret = (local_or_remote == 1 ? getsockname(s, (struct sockaddr *)&addr, (socklen_t*)&addrlen) :
            getpeername(s, (struct sockaddr *)&addr, (socklen_t*)&addrlen));
    if (ret < 0)
        perror("getsock(peer)name");
    return addr;
}


char* get_ip_address(int s, int local_or_remote) {
    struct sockaddr_in addr;
    addr = get_socket_name(s, local_or_remote);
    return inet_ntoa(addr.sin_addr);
}


int get_port(int s, int local_or_remote) {
    struct sockaddr_in addr;
    addr = get_socket_name(s, local_or_remote);
    return addr.sin_port;
}


void send_to_all(char* buf, int nbytes) {
    int j, ret;
    for(j = 0; j <= fdmax; j++) {
        if (FD_ISSET(j, &master))
            if (j != listener && j != crt)
                if ( send(j, buf, nbytes, 0) == -1)
                    perror("send");
    }
    return;
}


int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <port number>\n", argv[0]);
        exit(1);
    }

    int_port = atoi(argv[1]);

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket error!");
        exit(1);
    }

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("Setsockopt error!");
        exit(1);
    }

    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(int_port);

    if (bind(listener, (struct sockaddr *)&myaddr, sizeof(myaddr)) == -1) {
        perror("Bind error!");
        exit(1);
    }

    if (listen(listener, 10) == -1) {
        perror("Listen error!");
        exit(1);
    }

    FD_SET(listener, &master);
    fdmax = listener;

    for(;;) {
        read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("Select error!");
            exit(1);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                crt = i;
                if (i == listener) {
                    addrlen = sizeof(remoteaddr);
                    if ((newfd = accept(listener, (struct sockaddr *)&remoteaddr,(socklen_t*)&addrlen)) == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the maximum
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s on "
                            "socket %d\n", get_ip_address(newfd, 0), newfd);
                        client_count++;
                        sprintf(buf,"Hi-you are client :[%d] (%s:%d) connected to server %s\nThere are %d clients connected\n",
                            newfd, get_ip_address(newfd, 0), get_port(newfd, 0),
                            get_ip_address(listener, 1), client_count);
                        send(newfd,buf,strlen(buf)+1,0);
                    }
                } else {
                    if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
                        if (nbytes == 0) {
                            printf("<selectserver>: client %d forcibly hung up\n", i);
                        } else perror("recv");
                        client_count--;
                        close(i);
                        FD_CLR(i, &master);
                    } else {
                        buf[nbytes]=0;
                        if ((strncasecmp("QUIT\n",buf,4) == 0)) {
                            sprintf(buf, "Request granted [%d] - %s. Disconnecting...\n", i, get_ip_address(i, 0));
                            send(i, buf, strlen(buf)+1,0);
                            nbytes = sprintf(tmpbuf, "<%s - [%d]> disconnected\n", get_ip_address(i, 0), i);
                            send_to_all(tmpbuf, nbytes);
                            client_count--;
                            close(i);
                            FD_CLR(i, &master);
                        } else {
                            nbytes = sprintf(tmpbuf, "<%s - [%d]> %s",get_ip_address(crt, 0),crt, buf);
                            send_to_all(tmpbuf, nbytes);
                        }
                    }
                }
            }
        }
    }
    return 0;
}
