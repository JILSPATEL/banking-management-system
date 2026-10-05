# ⭐ Low Priority — Supporting OS Concepts
> **Priority: LOW** — Know what these are, where they're used, and 1-2 interview points each

---

## 1. Time API — time(), localtime(), struct tm

### What It Is
POSIX time functions for getting the current time and formatting it for display.

### Why It Exists in This Project
Transaction records need timestamps — when a deposit/withdrawal/transfer occurred.

### Where It's Used

| File | Function | Variables |
|------|----------|----------|
| `Customer.h` | `depositMoney()` | `time(NULL)`, `localtime(&s)`, `current_time->tm_hour/min/sec/year/mon/mday` |
| `Customer.h` | `withdrawMoney()` | Same pattern |
| `Customer.h` | `transferFunds()` | Same pattern |
| `Employee.h` | `addCustomer()` | Same pattern — timestamps opening balance |
| `Employee.h` | `approveRejectLoan()` | Same pattern — timestamps loan approval |

### How It Works
```c
time_t s = time(NULL);           // Seconds since Jan 1, 1970 (Unix epoch)
struct tm *current_time = localtime(&s);  // Convert to local time struct
// Access fields: tm_hour, tm_min, tm_sec, tm_year (+1900), tm_mon (+1), tm_mday
```

### Interview Points
- `time_t` is typically a 64-bit integer (seconds since Unix epoch = Jan 1, 1970 UTC)
- **Year 2038 problem:** 32-bit `time_t` overflows in January 2038. Modern 64-bit systems are immune.
- `localtime()` is NOT thread-safe — it returns a pointer to a static struct shared across calls. Thread-safe version: `localtime_r()`. Not an issue here since each child process handles one client.

---

## 2. Byte Order Conversion — htons(), htonl(), inet_addr()

### What It Is
Network communication requires a standard byte order. **Network byte order** is **big-endian**. x86 CPUs use **little-endian**. Conversion functions bridge this gap.

### Where It's Used

| File | Function | Call | Purpose |
|------|----------|------|---------|
| `server.c` | `main()` | `htons(8080)` | Convert port to network byte order |
| `server.c` | `main()` | `htonl(INADDR_ANY)` | Convert IP address to network byte order |
| `client.c` | `main()` | `htons(8080)` | Same — client side |
| `client.c` | `main()` | `inet_addr("127.0.0.1")` | Convert dotted-decimal string to 32-bit binary |

### Conversion Functions

| Function | Converts | Direction |
|----------|----------|-----------|
| `htons()` | 16-bit (short) | Host → Network (big-endian) |
| `htonl()` | 32-bit (long) | Host → Network (big-endian) |
| `ntohs()` | 16-bit (short) | Network → Host |
| `ntohl()` | 32-bit (long) | Network → Host |
| `inet_addr()` | Dotted-decimal string | → 32-bit network byte order |

### Interview Points
- On a big-endian machine (e.g., SPARC, older PowerPC), `htons()` is a no-op — host and network order are the same
- On little-endian (x86/x64), `htons(8080)` = `0x901F` (bytes swapped from `0x1F90`)
- Not converting byte order is a classic bug when porting network code between architectures

---

## 3. SO_REUSEADDR — Socket Option

### What It Is
A socket option that allows binding to a port still in the TIME_WAIT state.

### Where It's Used

| File | Function | Call |
|------|----------|------|
| `server.c` | `main()` | `setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))` |

### Why TIME_WAIT Exists
After a TCP connection is closed, the port remains in TIME_WAIT for 2×MSL (Maximum Segment Lifetime, typically 60-120 seconds). This ensures delayed packets from the old connection don't corrupt a new connection on the same port.

### Interview Points
- Without `SO_REUSEADDR`, after stopping the server with Ctrl+C, `bind()` would fail with "Address already in use" for up to 2 minutes
- `SO_REUSEPORT` (different option) allows multiple sockets to bind to the same port — used for load balancing across processes

---

## 4. /dev/shm — Shared Memory Filesystem

### What It Is
`/dev/shm` is a **tmpfs** filesystem mounted in RAM. Files here are not persisted to disk and are fast to access.

### Where It's Used
POSIX named semaphores are stored here automatically:
- Semaphore name `/sem_1001` → file `/dev/shm/sem.sem_1001`
- `cleanupAllSemaphores()` in `server.c` directly manipulates files in `/dev/shm/`

### Why RAM-Based
Semaphore operations (`sem_wait`, `sem_post`) must be extremely fast — they're in the critical path of every login. Storing them in RAM instead of a spinning disk makes them effectively instant.

### Interview Points
- tmpfs (temporary filesystem) lives in RAM — no disk I/O
- Files in /dev/shm are lost on reboot (expected for semaphores)
- Maximum size of /dev/shm is configurable — usually half of physical RAM

---

## 5. bzero() / memset() — Buffer Initialization

### What It Is
`bzero(buf, n)` fills `n` bytes of `buf` with zeros. It's the equivalent of `memset(buf, 0, n)`. `bzero()` is deprecated in POSIX (use `memset`), but still widely used.

### Where It's Used
`bzero()` is used extensively before every `read()` and `write()` call throughout the project:
```c
bzero(readBuffer, sizeof(readBuffer));
bzero(writeBuffer, sizeof(writeBuffer));
```

### Why Before Every read()/write()
- `readBuffer` is reused across multiple requests. Without zeroing, old data remains after a short read
- `writeBuffer` is zeroed to ensure no stale content leaks into the next message
- Prevents subtle bugs where partial overwrites leave old data at the end of the buffer

### Interview Points
- `bzero()` is deprecated — `memset(buf, 0, n)` is preferred in new code
- For security-sensitive data (passwords), the buffer should be zeroed after use too — prevents the data from leaking in stack dumps
- `explicit_bzero()` (Linux) or `memset_s()` (C11) prevent the compiler from optimizing out the zero-fill (compilers may remove "unnecessary" bzero calls)

---

## 6. getpid() — Process Identification in File Locks

### What It Is
`getpid()` returns the PID (Process ID) of the calling process.

### Where It's Used
`getpid()` is used to set the `l_pid` field in `struct flock` before calling `fcntl()` for file locking.

```c
struct flock fl = {F_WRLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};
```

### Interview Points
- The `l_pid` field in `struct flock` is populated by the kernel when **querying** a lock (F_GETLK) to identify who holds the lock. When **setting** a lock (F_SETLK/F_SETLKW), the kernel fills `l_pid` automatically — setting it yourself is informational only.
- `getppid()` returns the parent process's PID

---

## 7. Makefile and Compilation Flags

### What It Is
A `Makefile` automates the build process. `make` reads it and runs the appropriate compile commands.

### Project Makefile
```makefile
server: server.c AllStructure/allStruct.h Modules/Customer.h ...
    gcc server.c -o server -pthread -lcrypt -lrt

client: client.c
    gcc client.c -o client
```

### Compilation Flags Explained

| Flag | Library | Purpose |
|------|---------|---------|
| `-pthread` | POSIX threads | Required for POSIX named semaphores (`sem_open`, etc.) even though threads aren't used |
| `-lcrypt` | libcrypt | Links the crypt library for password hashing |
| `-lrt` | librt (real-time) | May be needed for `sem_open` on older glibc versions |

### Why -pthread Even Without Threads?
POSIX semaphore functions (`sem_open`, `sem_post`, etc.) are defined in the pthreads library (`-lpthread`). On Linux, `-pthread` both links libpthread AND sets compiler defines for thread-safe behavior.

### Interview Points
- The order of flags matters in GCC: `-lpthread` should come after the source files
- `-lrt` is the POSIX real-time extensions library; newer glibc versions include semaphore support directly in `-lc`, making `-lrt` optional
- `make clean` removes compiled binaries: `rm -f server client`

---

## 8. goto Statement — Re-Login Flow

### What It Is
`goto` is a C control flow statement that unconditionally jumps to a label within the same function.

### Where It's Used

| File | Function | Purpose |
|------|----------|---------|
| `Customer.h` | `customerMenu()` | `goto label1` — return to account number prompt after password change |
| `Employee.h` | `employeeMenu()` | `goto label1` — return to employee ID prompt after password change |
| `Manager.h` | `managerMenu()` | `goto label1` — return to manager ID prompt after password change |

### Why Used Here
After a user changes their password, the session must restart (re-authentication required). The `goto label1` jumps back to the top of the function where credentials are requested. The alternative would be a loop with a control variable.

### Interview Points
- `goto` is generally discouraged in structured programming (Dijkstra's "Go To Statement Considered Harmful", 1968)
- Acceptable uses: error cleanup (jumping to cleanup code at end of function), breaking out of nested loops
- In this project, a `while(1)` loop with a boolean flag (like `relogin = 1; continue;`) would be the preferred structured alternative

---

## 9. setsockopt() — Socket Option Configuration

### What It Is
`setsockopt()` configures options on a socket. Used at the socket level or protocol level.

### Where It's Used

| File | Function | Option | Value |
|------|----------|--------|-------|
| `server.c` | `main()` | `SO_REUSEADDR` at `SOL_SOCKET` level | 1 (enabled) |

### Interview Points
- `SOL_SOCKET` = socket-level option; `IPPROTO_TCP` = TCP-level option
- Other useful options: `SO_KEEPALIVE` (detect dead connections), `TCP_NODELAY` (disable Nagle's algorithm), `SO_RCVBUF`/`SO_SNDBUF` (buffer sizes)

---

## 10. ftruncate() and File Truncation

### What It Is
`ftruncate(fd, length)` sets the file size to exactly `length` bytes.

### Where It's Used

| File | Function | Call | Purpose |
|------|----------|------|---------|
| `Customer.h` | `applyLoan()` | `ftruncate(file, sizeof(ct))` | Ensure `loanCounter.txt` stays exactly `sizeof(struct Counter)` bytes |

### Why Needed
The loan counter file stores one struct. If previous reads/writes left the file offset beyond the struct, writing the struct might not shrink the file. `ftruncate()` enforces the exact file size.

### Interview Points
- If the file is shorter than the truncation size, it's extended with null bytes
- Must use `ftruncate()` (POSIX, works on fd) not `truncate()` (works on path) when you have the file already open

---

## 📌 Quick Revision Table

| Concept | File | Key API | One-Line Purpose |
|---------|------|---------|-----------------|
| Time/timestamps | `Customer.h`, `Employee.h` | `time()`, `localtime()` | Record when transactions happen |
| Byte order | `server.c`, `client.c` | `htons()`, `htonl()`, `inet_addr()` | Ensure network data is in big-endian |
| SO_REUSEADDR | `server.c` | `setsockopt()` | Allow port reuse after server restart |
| /dev/shm | `server.c` | `opendir()`, `unlink()` | Clean stale semaphores on startup |
| `bzero()` | All modules | `bzero(buf, size)` | Zero out reused buffers before each use |
| `getpid()` | Multiple | `getpid()` | Identify process in file lock structs |
| Makefile | `Makefile` | `make`, `make clean` | Automate compilation with correct flags |
| `goto` | `Customer/Employee/Manager.h` | `goto label1` | Jump back to re-login after password change |
| `setsockopt` | `server.c` | `setsockopt()` | Configure socket-level options |
| `ftruncate` | `Customer.h` | `ftruncate()` | Enforce exact file size for counter file |

**Frequently Confused:**
- `time()` (seconds since epoch) vs `clock()` (CPU time used by process) — `time()` is for wall-clock timestamps
- `htons` (port, 16-bit) vs `htonl` (IP, 32-bit) — use the right size
- `bzero` (deprecated) vs `memset` (current standard) — prefer memset in new code
