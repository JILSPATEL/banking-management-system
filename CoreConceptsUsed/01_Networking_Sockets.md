# ⭐⭐⭐ Networking & TCP Socket Programming
> **Priority: HIGH** — Most commonly asked topic in OS/Linux/Systems interviews

---

## 1. Client-Server Architecture

### Concept Overview
A **Client-Server Architecture** separates responsibilities: the server provides services (banking operations) and clients consume them. The server runs continuously, accepting connections from multiple clients.

### Why Used in This Project
The Banking Management System needs a centralized server because:
- All account data (customers.txt, employees.txt) must be shared across multiple concurrent users
- Business logic (deposit, withdraw, loan approval) must happen in one controlled place
- A single server guarantees data consistency — no client holds its own copy of bank data

### Project Mapping

| Feature | Filename | Function | Purpose |
|---------|----------|----------|---------|
| Server startup | `server.c` | `main()` | Creates socket, binds, listens |
| Client startup | `client.c` | `main()` | Creates socket, connects to server |
| Request dispatch | `server.c` | `connectionHandler()` | Reads client choice, routes to module |
| Client I/O loop | `client.c` | `connectionHandler()` | Reads server prompt, sends user input |

### Execution Flow
```
server.c main()
  → socket()           # Create TCP socket
  → setsockopt()       # Set SO_REUSEADDR
  → bind()             # Bind to port 8080
  → listen()           # Mark socket as passive
  → accept() [loop]    # Block until client connects
  → fork()             # Spawn child process per client
  → connectionHandler() # Child handles all I/O

client.c main()
  → socket()           # Create TCP socket
  → connect()          # Connect to 127.0.0.1:8080
  → connectionHandler() # Read/write loop with server
```

---

## 2. TCP/IP Socket Programming — The Full Lifecycle

### Concept Overview
A **socket** is a communication endpoint. TCP (Transmission Control Protocol) provides reliable, ordered, connection-oriented data delivery. IP (Internet Protocol) handles addressing and routing.

### Why Used in This Project
- TCP guarantees that banking data (account numbers, amounts, passwords) arrives **complete and in order** — critical for financial operations
- Connection-oriented means the server knows when a client disconnects
- TCP is the industry standard for client-server applications requiring reliability

### Internal Working (Interview Level)

#### TCP Connection Establishment — 3-Way Handshake
```
Client                    Server
  │──── SYN ────────────►│
  │◄─── SYN-ACK ─────────│
  │──── ACK ────────────►│
  │   [Connection OPEN]   │
```

#### TCP Connection Teardown — 4-Way Handshake
```
Client                    Server
  │──── FIN ────────────►│
  │◄─── ACK ─────────────│
  │◄─── FIN ─────────────│
  │──── ACK ────────────►│
  │   [Connection CLOSED] │
```

---

## 3. socket() — Creating a Socket

### Concept Overview
`socket()` creates a new communication endpoint and returns a **file descriptor** (an integer). Everything in Linux is a file, and sockets follow the same principle.

### Project Mapping

| File | Call | Parameters Explained |
|------|------|---------------------|
| `server.c` `main()` | `socket(AF_INET, SOCK_STREAM, 0)` | AF_INET=IPv4, SOCK_STREAM=TCP, 0=auto protocol |
| `client.c` `main()` | `socket(AF_INET, SOCK_STREAM, 0)` | Same — creates client-side TCP socket |

### Parameter Deep Dive
- **`AF_INET`** — Address Family IPv4. Use `AF_INET6` for IPv6 or `AF_UNIX` for local sockets
- **`SOCK_STREAM`** — Stream socket = TCP. `SOCK_DGRAM` would give UDP
- **`0`** — Protocol auto-selected based on type (TCP for STREAM)

### Return Value
- Success: A non-negative integer (file descriptor)
- Failure: `-1` with `errno` set (checked with `perror()` in project)

---

## 4. bind() — Assigning an Address to a Socket

### Concept Overview
`bind()` associates a socket with a specific IP address and port number. Without `bind()`, the OS would assign a random port and the client would not know where to connect.

### Project Mapping

| File | Call | Purpose |
|------|------|---------|
| `server.c` `main()` | `bind(socketFileDescriptor, &address, sizeof(address))` | Binds to `0.0.0.0:8080` |

### Address Structure Setup
```
server.c:
  address.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on ALL interfaces
  address.sin_family      = AF_INET;              // IPv4
  address.sin_port        = htons(8080);          // Port 8080
```

- **`INADDR_ANY`** — Bind to all available network interfaces (0.0.0.0). If the machine has multiple NICs, it accepts on all.
- **`htonl()`** — Host to Network Long: converts 32-bit integer to big-endian (network byte order)
- **`htons()`** — Host to Network Short: converts 16-bit port to big-endian

### Why Port 8080?
Port 8080 is above 1024 (privileged range), so it doesn't require root access. Port 80 (HTTP) or 443 (HTTPS) require `sudo`.

### Common Interview Questions

**Q: What is the difference between bind() and connect()?**
> `bind()` is called by the server to assign a local address. `connect()` is called by the client to initiate a connection to a remote address.

**Q: Can two processes bind to the same port?**
> Not without `SO_REUSEADDR`. With `SO_REUSEADDR`, a new server can bind to a port that is in TIME_WAIT state from a previous server instance.

---

## 5. listen() — Marking a Socket as Passive

### Concept Overview
`listen()` converts an active socket into a passive socket — one that waits for incoming connections rather than initiating them. It also sets the size of the **connection backlog queue**.

### Project Mapping

| File | Call | Parameters |
|------|------|-----------|
| `server.c` `main()` | `listen(socketFileDescriptor, 2)` | Backlog = 2 pending connections |

### The Backlog Queue
When `listen(fd, 2)` is called:
- The OS maintains a queue of up to 2 **completed** TCP connections waiting to be `accept()`-ed
- If a 3rd client tries to connect while the queue is full, the connection is either queued in the SYN queue or refused
- In a production system, this backlog is set much higher (e.g., 128 or `SOMAXCONN`)

**Why backlog=2 in this project?** This is a demonstration system. In production banking software, this would be `SOMAXCONN` (typically 128 on Linux).

---

## 6. accept() — Accepting an Incoming Connection

### Concept Overview
`accept()` extracts the first connection from the completed connection queue, creates a **new socket** for that specific client, and returns its file descriptor. The original listening socket continues to accept more connections.

### Project Mapping

| File | Call | Variables |
|------|------|----------|
| `server.c` `main()` | `accept(socketFileDescriptor, &client, &clientSize)` | `connectionFileDescriptor` = new fd per client |

### Two Sockets in the Server
This is a key interview concept:
```
socketFileDescriptor      ← Listening socket (stays open forever)
connectionFileDescriptor  ← New socket for THIS specific client
```

- `socketFileDescriptor` keeps listening for new clients
- `connectionFileDescriptor` is passed to `connectionHandler()` (in the child process after `fork()`)
- When the client disconnects, `connectionFileDescriptor` is closed. `socketFileDescriptor` is NOT closed.

### Blocking Behavior
`accept()` **blocks** (the process sleeps) until a client connects. This is **blocking I/O** — the calling thread/process waits with no CPU consumption.

---

## 7. connect() — Client Initiates Connection

### Concept Overview
`connect()` initiates a TCP connection from client to server. It triggers the 3-way handshake.

### Project Mapping

| File | Call | Target |
|------|------|--------|
| `client.c` `main()` | `connect(socketFileDescriptor, &address, sizeof(address))` | `127.0.0.1:8080` |

```c
// client.c
address.sin_addr.s_addr = inet_addr("127.0.0.1");  // Loopback — same machine
address.sin_family      = AF_INET;
address.sin_port        = htons(8080);
```

- **`inet_addr("127.0.0.1")`** — Converts a dotted-decimal IP string to a 32-bit binary in network byte order
- **`127.0.0.1`** — The loopback address. Traffic stays within the same machine, never goes to a real network

---

## 8. read() and write() on Sockets

### Concept Overview
In Linux, sockets are file descriptors. The same `read()` and `write()` system calls used for files work on sockets too. This is the **"everything is a file"** philosophy.

### Project Mapping

| Operation | File | Function | Purpose |
|-----------|------|----------|---------|
| Server sends menu | `server.c` | `connectionHandler()` | `write(connectionFD, MAINMENU, ...)` |
| Client reads menu | `client.c` | `connectionHandler()` | `read(socketFD, readBuffer, ...)` |
| Client sends choice | `client.c` | `connectionHandler()` | `write(socketFD, writeBuffer, ...)` |
| Server reads choice | `server.c` | `connectionHandler()` | `read(connectionFD, readBuffer, ...)` |

### The Request-Response Pattern
Every operation in this project follows a strict **ping-pong** protocol:
```
Server writes prompt  →  Client reads, displays to user
Client reads input    →  Client writes to server
Server reads choice   →  Server processes, writes result
Client reads result   →  displays, sends empty ACK back
```

The `^` character at the end of messages is a **custom protocol delimiter** — when the client sees a message ending in `^`, it displays the content and sends an empty acknowledgment without waiting for user input.

### Blocking vs Non-Blocking I/O
- `read()` on a socket **blocks** until data arrives from the other end
- This project uses **blocking I/O** (the default)
- **Non-blocking I/O** (`O_NONBLOCK`) would return immediately with `EAGAIN` if no data is available
- **Alternative:** `select()`, `poll()`, or `epoll()` for multiplexing — not used here because `fork()` gives each client its own process

---

## 9. SO_REUSEADDR — Socket Option

### Concept Overview
After a server is stopped and restarted, the port may remain in **TIME_WAIT** state for up to 60 seconds. Without `SO_REUSEADDR`, `bind()` would fail with "Address already in use."

### Project Mapping

| File | Call | Purpose |
|------|------|---------|
| `server.c` `main()` | `setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))` | Allow immediate port reuse after restart |

### Why TIME_WAIT Exists
TIME_WAIT ensures all delayed TCP packets from the old connection are gone before the port is reused. `SO_REUSEADDR` allows binding while in TIME_WAIT.

---

## 10. Common Interview Questions

**Q1: Explain the difference between TCP and UDP. Why did you use TCP?**
> TCP provides reliable, ordered, error-checked delivery. UDP is connectionless and has no delivery guarantees. For banking data — account numbers, balances, passwords — we cannot afford packet loss or reordering. TCP's reliability guarantees data integrity even if the network is unreliable.

**Q2: What is the difference between socket() and socketpair()?**
> `socket()` creates a single endpoint that must be connected to another socket (on the same or different machine). `socketpair()` creates two already-connected sockets — used for IPC on the same machine. This project uses `socket()` because it needs to support network-based clients.

**Q3: What happens if accept() fails?**
> In this project, the error is printed with `perror()` but the server continues (no `exit()`). This is intentional — a single failed accept should not crash the entire server. Only socket creation, bind, and listen failures cause `exit(-1)`.

**Q4: Why is the connection file descriptor passed to the child process after fork()?**
> After `fork()`, both parent and child inherit all open file descriptors. The child uses `connectionFileDescriptor` to communicate with its assigned client. The parent should `close(connectionFileDescriptor)` to avoid fd leaks — this project does not explicitly do that, which is a known improvement point.

**Q5: What is the significance of 127.0.0.1 in the client?**
> 127.0.0.1 is the IPv4 loopback address. Traffic sent to it never leaves the machine — it's routed back to the same host. This means the client and server must run on the same machine. For a distributed deployment, replace it with the actual server IP and configure firewall rules.

**Q6: What is the backlog parameter in listen()?**
> The backlog is the maximum number of pending connections in the kernel's completed connection queue. Connections beyond this limit may be dropped. This project uses `2`, which is low for a real bank but sufficient for a demo.

**Q7: Can read() return less bytes than requested?**
> Yes. TCP is a stream protocol — it does not preserve message boundaries. `read()` may return partial data if the sender sent it in chunks or if the buffer is smaller than the message. This project uses fixed-size 4096-byte buffers, which works for small messages but could fail with large transaction histories in production.

---

## 11. Follow-up Questions

> **Q: Why not use threads instead of fork() for multiple clients?**
> → See `02_Process_Management.md` — fork() provides process isolation. If one client's process crashes (SIGSEGV), other clients are unaffected. With threads, a crash in one thread kills the entire server.

> **Q: Why blocking I/O instead of epoll?**
> → With fork(), each child process handles exactly one client, so there's no need for multiplexing. epoll would be needed in a single-process/single-threaded server handling many clients. The trade-off is process overhead vs. event-loop complexity.

> **Q: What would change to make this work over the internet?**
> → Replace `127.0.0.1` with the actual IP in `client.c`, add TLS/SSL (e.g., OpenSSL) for encryption, increase the backlog, add authentication tokens, and implement proper session management.

---

## 12. Common Mistakes in Interviews

- Confusing the **listening socket** with the **connection socket** — they are two different fd's
- Saying `connect()` is called by the server — it's always the **client**
- Saying `accept()` blocks forever — it blocks until a client connects, then returns immediately
- Forgetting that `read()`/`write()` on sockets can return partial data
- Confusing TCP with HTTP — HTTP is an **application-layer protocol** that runs **on top of** TCP

---

## 13. Best Practices

- Always call `setsockopt(SO_REUSEADDR)` before `bind()` in server development
- Close unused file descriptors after `fork()` — the parent should close `connectionFileDescriptor`
- Check return values of every socket system call — banking data cannot be lost silently
- Use `perror()` with a meaningful prefix to get context-aware error messages

---

## 📌 Revision Box — One-Liners

| Concept | One-Line Explanation |
|---------|---------------------|
| `socket()` | Creates a file descriptor representing a communication endpoint |
| `bind()` | Assigns a local IP+port to the server socket |
| `listen()` | Marks socket as passive; sets queue size for pending connections |
| `accept()` | Dequeues one completed connection; returns a new per-client socket fd |
| `connect()` | Client-side call that triggers the TCP 3-way handshake |
| `read()`/`write()` | Same syscalls as file I/O — sockets are file descriptors |
| `SO_REUSEADDR` | Allows binding to a port still in TIME_WAIT state |
| `INADDR_ANY` | Server listens on all available network interfaces |
| TCP | Reliable, ordered, connection-oriented — mandatory for financial data |
| `127.0.0.1` | Loopback address — traffic stays on the same machine |

**Frequently Confused:**
- `listen()` backlog ≠ maximum number of clients. It's the pending queue before `accept()`
- `AF_INET` (address family) ≠ `PF_INET` (protocol family) — historically different, practically identical today
- `htons()` converts **port** (16-bit short); `htonl()` converts **IP address** (32-bit long)
