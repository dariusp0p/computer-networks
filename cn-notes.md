# CN Notes

## Some Theory

### IPs
* IPv4 = 32 bits → A.B.C.D (e.g. 192.168.0.15)
* Public IP: globally unique, routable on the Internet
* Private IP: used only inside local networks (not routable globally)

1. Devices inside a LAN get private IPs from the router (via DHCP). 
2. The router has a public IP on its WAN side — unique globally. 
3. When you connect to a website, the router performs NAT
4. The Internet (e.g. Google) only sees the router’s public IP, not the internal ones.


## 0. Socket Creation
* needed on both sides
* AF_INET -> IPv4
* SOCK_STREAM -> TCP
* SOCK_DGRAM -> UDP
* 0 -> choose default protocol

```
// TCP
int sock = socket(AF_INET, SOCK_STREAM, 0);
if (sock < 0) {
    perror("socket");
    exit(1);
}

// UDP
int sock = socket(AF_INET, SOCK_DGRAM, 0);
if (sock < 0) {
    perror("socket");
    exit(1);
}
```

```
# TCP
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# UDP
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
```


## 1. Binding
* bind() associates a socket descriptor with a (or more) specific IP address/es and port on the local machine.
* It’s required on the server side (so clients know where to connect).
* The client usually don't — the OS assigns a random ephemeral port automatically.
* Works the same for both TCP and UDP.

```int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);```
* sockfd → socket file descriptor (created with socket())
* addr → pointer to a struct describing the local address
* addrlen → size of that struct

```
struct sockaddr_in server_addr;
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(12345);       // port number in network byte order
server_addr.sin_addr.s_addr = INADDR_ANY;  // listen on all interfaces
memset(&(server_addr.sin_zero), 0, 8);     // clear padding

if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("bind");
    exit(1);
}
```

```
sock.bind(("0.0.0.0", 12345)) 
```


## 2. Listen
* on the server side, only needed for TCP
```int listen(int sockfd, int backlog);```
* sockfd -> The bound socket file descriptor
* backlog -> Max number of pending connections waiting to be accepted
* It doesn't block

```
listen(sock, 5);
```

```
sock.listen(5)
```


## 3. Connect
* connect() is used by a client to establish a TCP connection.
* Can be used with UDP if you want to use the send() recv() (but not necessary).
* By default it is blocking

```int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);```
* sockfd -> Socket descriptor (from socket())
* addr -> Destination address and port 
* addrlen -> Size of that struct 
* Returns 0 on success, -1 on error (check errno)

```
connect(sock, (struct sockaddr *) &server, sizeof(server));
```

```
sock.connect(("192.168.0.10", 12345))
```


## 4. Accept
* on the server side, only needed for TCP
* Removes one established connection from the listening socket’s accept queue and returns a new socket dedicated to that client.
* The listening socket stays open to accept more clients.

* Blocking (default): accept() waits until a connection is ready.
* Non-blocking: set O_NONBLOCK (C) or setblocking(False) (Python):

```
int accept(int listen_fd, struct sockaddr *addr, socklen_t *addrlen);
```
* listen_fd -> a TCP socket that was bind() + listen()-ed
* addr -> (out) peer address (can be NULL)
* addrlen -> (in/out) size of addr
* Returns client_fd (≥ 0) on success, -1 on error

```
int client = accept(sock, NULL, NULL);
```

```
client_sock, client_addr = sock.accept()
```


## 5. Data Transfer

### htons() / ntohs() / htonl() / ntohl()
* Computers store multi-byte numbers in different byte orders depending on CPU architecture:
  * Intel / AMD (x86, x64) -> Little-endian 
  * Many network devices (routers, protocols) -> Big-endian
* But all Internet protocols (TCP/IP, UDP, etc.) define network byte order = big-endian.

  * htons() - 16 bits - Host → Network
  * ntohs() - 16 bits - Network → Host 
  * htonl() - 32 bits - Host → Network 
  * ntohl() - 32 bits - Network → Host

### a) send() / recv() (TCP)
```ssize_t send(int sockfd, const void *buf, size_t len, int flags);```
* Sends up to len bytes from buf through an established TCP connection.
* May send fewer bytes than requested — always check return value.
* Blocks (by default) until some data is accepted by the kernel buffer.
* Use flags = 0 for normal sending.

```ssize_t recv(int sockfd, void *buf, size_t len, int flags);```
* Reads available data from the TCP socket.
* Blocks until at least one byte is available or the connection closes.
* Returns: > 0 → bytes received; 0 → connection closed (EOF); < 0 → error (EAGAIN, EWOULDBLOCK, ECONNRESET…)

```
data = sock.recv(1024)
sock.sendall(b"Hello") # automatically loops until all data is sent
```

### b) sendto() / recvfrom() (UDP)
```
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
```
* Sends one datagram to the given destination address.
* The entire datagram is sent in one call (or not at all).

```
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
```
* Receives one UDP datagram and fills src_addr with the sender’s info.

```
data, addr = sock.recvfrom(1024)
sock.sendto(b"pong", addr)
```

### TCP vs UDP
#### TCP:
1. reliable
2. preserves order
3. retransmits lost data
4. discards duplicate data
5. prevents buffer overflow
6. used for reliable data (web, ssh, docs)
#### UDP:
1. enables broadcasting
2. faster speed
3. used for real-time data (games, voice, video)

## 6. Select() - I/O Multiplexing
Normally, blocking socket calls (recv(), accept(), etc.) freeze your program until data arrives. If you have multiple sockets, you’d need multiple threads or processes to handle them simultaneously — unless you use I/O multiplexing.

select() let one process monitor many sockets at once and only act when one becomes ready.

```
int select(int nfds,
           fd_set *readfds,
           fd_set *writefds,
           fd_set *exceptfds,
           struct timeval *timeout);
```
* nfds -> Highest FD + 1 
* readfds -> Sockets to check for readable data 
* writefds -> Sockets to check for writable buffer space 
* exceptfds -> Check for exceptions (rarely used)
* timeout -> How long to wait (NULL = forever)

Supporting macros:
```
FD_ZERO(&set);        // initialize empty set
FD_SET(fd, &set);     // add socket to set
FD_CLR(fd, &set);     // remove socket
FD_ISSET(fd, &set);   // check if socket is ready
```

Typical example:
```
fd_set master, readfds;
FD_ZERO(&master);
FD_SET(listener, &master);
int maxfd = listener;

for (;;) {
    readfds = master; // copy each time!
    select(maxfd + 1, &readfds, NULL, NULL, NULL);

    for (int fd = 0; fd <= maxfd; fd++) {
        if (FD_ISSET(fd, &readfds)) {
            if (fd == listener) {
                // new connection
                int client = accept(listener, NULL, NULL);
                FD_SET(client, &master);
                if (client > maxfd) maxfd = client;
            } else {
                // data from existing client
                char buf[1024];
                int n = recv(fd, buf, sizeof(buf), 0);
                if (n <= 0) {
                    close(fd);
                    FD_CLR(fd, &master);
                } else {
                    send(fd, buf, n, 0);
                }
            }
        }
    }
}
```

```
import select, socket

srv = socket.socket()
srv.bind(("0.0.0.0", 12345))
srv.listen()

clients = [srv]

while True:
    readable, _, _ = select.select(clients, [], [])
    for s in readable:
        if s is srv:
            cli, addr = srv.accept()
            clients.append(cli)
        else:
            data = s.recv(1024)
            if not data:
                clients.remove(s)
                s.close()
            else:
                s.sendall(data)
```

## 7. Threading & Multiprocessing (good old OS)
select() works great for moderate numbers of sockets, but handling CPU-intensive tasks, blocking disk I/O or very large numbers of clients may require true concurrency — multiple threads or processes running in parallel.

### Threading
* A thread is a lightweight execution unit within a process.
* All threads share the same memory space and resources (variables, heap, sockets).
* Each thread has its own stack and instruction pointer.

```
void *handle_client(void *arg) {
    int client = *(int*)arg;
    char buf[1024];
    int n = recv(client, buf, sizeof(buf), 0);
    send(client, buf, n, 0);
    close(client);
    return NULL;
}

while (1) {
    int client = accept(listener, NULL, NULL);
    pthread_t t;
    pthread_create(&t, NULL, handle_client, &client);
    pthread_detach(t); // no need to join later
}
```

```
import socket, threading

def handle_client(cli):
    while True:
        data = cli.recv(1024)
        if not data: break
        cli.sendall(data)
    cli.close()

srv = socket.socket()
srv.bind(("0.0.0.0", 12345))
srv.listen()

while True:
    cli, addr = srv.accept()
    threading.Thread(target=handle_client, args=(cli,)).start()
```

### Multiprocessing
* Each process is a separate program instance with its own memory space. 
* They communicate via pipes, queues, or sockets.
* Much safer (no shared data), but heavier than threads.

```
while (1) {
    int client = accept(listener, NULL, NULL);
    if (fork() == 0) {
        // child process
        close(listener);
        handle_client(client);
        exit(0);
    }
    close(client); // parent closes copy
}
```

```
import socket, multiprocessing

def handle_client(cli):
    data = cli.recv(1024)
    cli.sendall(data)
    cli.close()

srv = socket.socket()
srv.bind(("0.0.0.0", 12345))
srv.listen()

while True:
    cli, _ = srv.accept()
    p = multiprocessing.Process(target=handle_client, args=(cli,))
    p.start()
```

### Locking Mechanisms
Threads share memory inside the same process.

If two threads read and write the same variable or structure at the same time → race condition.

#### Mutex
A mutex ensures only one thread at a time can execute a block of code.
```
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_lock(&lock);
// critical section (shared data access)
pthread_mutex_unlock(&lock);
```

```
lock = threading.Lock()

with lock:
    shared_resource += 1

# or
lock.acquire()
shared_resource += 1
lock.release()
```

#### Barrier
A barrier is a checkpoint where N threads must arrive before any of them can continue.
When the last thread reaches the barrier, all waiting threads are released simultaneously.

```
pthread_barrier_t barrier;
pthread_barrier_init(&barrier, NULL, THREADS);

pthread_barrier_wait(&barrier);

pthread_barrier_destroy(&barrier);
```

```
barrier = threading.Barrier(3)

barrier.wait()
```

#### Semaphore
A semaphore limits how many threads can access a shared resource at once.
It maintains a count:
* sem_wait() (or acquire()) → decreases count → blocks if 0
* sem_post() (or release()) → increases count → wakes a waiting thread

```
sem_t sem;
sem_init(&sem, 0, 3);  // init with count 3

sem_wait(&sem);        // acquire (decrement, blocks if 0)
critical_section();
sem_post(&sem);        // release (increment)
```

```
from threading import Semaphore
sem = Semaphore(3)

with sem:
    print("working...")

# or
sem.acquire()
critical_section()
sem.release()
```

#### Condition Variables
A condition variable lets threads wait for a specific event or state to become true, instead of constantly checking.
* Threads call wait() to sleep until another thread signal()s the condition.
* Always used with a mutex to protect the shared state being checked.

```
pthread_mutex_t lock;
pthread_cond_t cond;

pthread_mutex_lock(&lock);
while (!ready)
    pthread_cond_wait(&cond, &lock);
pthread_mutex_unlock(&lock);

// somewhere else:
pthread_mutex_lock(&lock);
ready = 1;
pthread_cond_signal(&cond);
pthread_mutex_unlock(&lock);
```

```
while not ready:
    cond.wait()

# elsewhere:
ready = True
cond.signal()
```


## 8. Cleanup
When a server finishes communication or shuts down, it must:
* release system resources (sockets, memory, threads),
* close connections gracefully so both ends know communication is ending,
* avoid resource leaks and half-open sockets.

```close(sockfd);```
```sock.close()```

Also don't forget about threads (if used)

```pthread_join(tid, NULL);```
```t.join()```


## Architectural Patterns for Socket Servers

### 1. Iterative (single-client)
* Flow: accept() → handle → close → repeat
* Pros: simplest, easy to debug
* Cons: one client at a time
* Use: labs, admin tools, offline batch protocols

```
import socket

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", 12345))
srv.listen(1)

while True:
    cli, addr = srv.accept()
    data = cli.recv(4096)
    if data:
        cli.sendall(data)  # echo
    cli.close()
```

### 2. Thread-per-client
* Flow: main thread accepts; spawn a thread for each client
* Pros: straightforward, blocking I/O is fine
* Cons: too many threads → high memory/context switches; needs locks
* Use: small/medium loads, I/O-bound handlers

```
import socket, threading

def handle(cli):
    with cli:
        while True:
            data = cli.recv(4096)
            if not data: break
            cli.sendall(data)

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", 12345))
srv.listen()

while True:
    cli, addr = srv.accept()
    threading.Thread(target=handle, args=(cli,), daemon=True).start()
```

### 3. Process-per-client (fork model)
* Flow: accept() then fork(); child handles client
* Pros: strong isolation; fewer locking issues
* Cons: heavier than threads; IPC needed for shared state 
* Use: security-sensitive handlers; legacy Unix style

```
import socket, os

def handle(cli):
    with cli:
        while True:
            data = cli.recv(4096)
            if not data:
                break
            cli.sendall(data)

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", 12345))
srv.listen()

while True:
    cli, addr = srv.accept()
    pid = os.fork()
    if pid == 0:  # child
        srv.close()          # child doesn’t need the listener
        handle(cli)
        os._exit(0)
    else:
        cli.close()          # parent closes its copy
```

### 4. Thread/Process Pool
* Flow: fixed N workers; main thread accepts and dispatches jobs (queue)
* Pros: bounded concurrency, amortized creation cost
* Cons: queue design matters; potential head-of-line blocking in pool
* Use: stable, predictable throughput servers

```
import socket
from concurrent.futures import ThreadPoolExecutor

def handle(cli):
    with cli:
        while True:
            data = cli.recv(4096)
            if not data: break
            cli.sendall(data)

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", 12345))
srv.listen()

with ThreadPoolExecutor(max_workers=8) as pool:
    while True:
        cli, addr = srv.accept()
        pool.submit(handle, cli)
```

### 5. Event-driven (I/O multiplexing) (with select)
* Flow: select() loop; non-blocking sockets; state machines per client
* Pros: single/few threads; scales to many idle connections
* Cons: more complex (stateful parsing, edge/level-trigger nuances)
* Use: chat, proxies, gateways, high-concurrency services

```
import socket, select

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", 12345))
srv.listen()
srv.setblocking(False)

sockets = {srv}
while True:
    readable, _, _ = select.select(sockets, [], [])
    for s in readable:
        if s is srv:
            cli, _ = srv.accept()
            cli.setblocking(False)
            sockets.add(cli)
        else:
            data = s.recv(4096)
            if not data:
                sockets.remove(s); s.close()
            else:
                s.sendall(data)
```

### 6. Hybrid
* Flow: event loop per core + worker pool for CPU-heavy tasks
* Pros: combines low-overhead I/O with true parallel CPU work
* Cons: scheduling and back-pressure design required
* Use: modern high-perf servers (reverse proxies, brokers)

```
import socket, select
from concurrent.futures import ThreadPoolExecutor

def process_and_reply(s, data):
    # heavy work here
    s.sendall(data)

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", 12345))
srv.listen()
srv.setblocking(False)

sockets = {srv}
with ThreadPoolExecutor(max_workers=8) as pool:
    while True:
        readable, _, _ = select.select(sockets, [], [])
        for s in readable:
            if s is srv:
                cli, _ = srv.accept()
                cli.setblocking(False)
                sockets.add(cli)
            else:
                data = s.recv(4096)
                if not data:
                    sockets.remove(s); s.close()
                else:
                    pool.submit(process_and_reply, s, data)
```

### 7. UDP Single-socket iterative loop
* Flow: recvfrom() → process → sendto()
* No threads needed

```
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 12345))

while True:
data, addr = sock.recvfrom(4096)
sock.sendto(data, addr)
```

## 8. UDP Thread pool
* Offload processing of datagrams to workers

```
import socket
from concurrent.futures import ThreadPoolExecutor

def process(sock, data, addr):
    reply = data.upper()
    sock.sendto(reply, addr)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 12345))

with ThreadPoolExecutor(max_workers=8) as pool:
    while True:
        data, addr = sock.recvfrom(4096)
        pool.submit(process, sock, data, addr)
```
