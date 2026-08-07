# ⭐⭐⭐ Process Management — fork(), Parent-Child, Multi-Client Server
> **Priority: HIGH** — fork() and process concepts are near-universal interview topics

---

## 1. Process Concept Overview

### What is a Process?
A **process** is a running instance of a program. It has its own:
- **PID** (Process ID)
- **Virtual address space** (stack, heap, code, data segments)
- **Open file descriptors**
- **Signal handlers**
- **Current working directory**

### Why Processes in This Project?
The Banking Management System server must handle **multiple clients simultaneously**. When a client connects, the server must continue accepting new clients without waiting for the first one to finish. Processes solve this.

---

## 2. fork() — Creating Child Processes

### Concept Overview
`fork()` creates an **exact copy** (child) of the calling process (parent). After `fork()`, both parent and child continue executing from the next line. The only difference is the **return value of fork()**.

### Project Mapping

| File | Function | Line Behaviour |
|------|----------|---------------|
| `server.c` | `main()` | `if(fork() == 0)` — child handles client |

### Return Value — The Critical Interview Point
```
fork() returns:
  0          → You are the CHILD process
  > 0 (PID)  → You are the PARENT process (value = child's PID)
  -1          → fork() failed (no memory / ulimit hit)
```

### How This Project Uses fork()
```
main() [server]
  └── accept() → connectionFileDescriptor = 5  (new client connected)
        └── fork()
              ├── Parent (fork() > 0):
              │     Goes back to accept() — waits for next client
              │     (Should close(connectionFileDescriptor) — fd leak exists)
              │
              └── Child (fork() == 0):
                    connectionHandler(connectionFileDescriptor)
                    Handles ALL operations for this one client
                    Exits when client disconnects or logs out
```

### Execution Flow for Multiple Clients

```
Server Process (PID 1000)
    accept() ← blocked...
    
Client 1 connects → fork() → Child PID 1001 handles Client 1
    accept() ← blocked...

Client 2 connects → fork() → Child PID 1002 handles Client 2
    accept() ← blocked...

Client 3 connects → fork() → Child PID 1003 handles Client 3
```

All three clients are served **concurrently** without any blocking each other.

---

## 3. Parent-Child Process Relationship

### Concept Overview
After `fork()`, the **parent** and **child** are separate processes with separate memory spaces. Changes in the child's memory do NOT affect the parent (or any other child).

### What the Child Inherits from the Parent
After `fork()`, the child gets a **copy** of:
- All open file descriptors (including `socketFileDescriptor` and `connectionFileDescriptor`)
- Signal handlers (important for semaphore cleanup)
- Global variables (`readBuffer`, `writeBuffer`, `sema`, `semName` in server.c)
- The current file offset of all open files

### What is NOT Shared
- **Memory** — child gets a copy (Copy-on-Write), not shared memory
- **PID** — child has a different PID
- **Semaphore locks held** — semaphores are per-process

### Copy-on-Write (COW) Optimization
Linux does NOT immediately copy all memory pages on `fork()`. Instead:
- Both parent and child **share the same physical pages** initially (marked read-only)
- When either process **writes** to a page, the OS creates a private copy for that process
- This makes `fork()` extremely fast even for large processes

---

## 4. File Descriptor Inheritance After fork()

### This Is Critical for This Project

After `fork()`:
- Both parent and child have `socketFileDescriptor` (listening socket) open
- Both parent and child have `connectionFileDescriptor` (client socket) open

**What should happen:**
- Parent: close `connectionFileDescriptor` (it doesn't need it)
- Child: close `socketFileDescriptor` (it doesn't need the listening socket)

**What this project does:**
- Child correctly uses `connectionFileDescriptor` for client communication
- Parent does NOT close `connectionFileDescriptor` → **file descriptor leak**
- Each accepted connection leaks one fd in the parent process

**Interview point:** This is a known improvement. In a long-running server with thousands of connections, the parent would eventually hit the per-process fd limit (default: 1024).

---

## 5. System Calls — What They Are

### Concept Overview
A **system call** is the interface between user-space programs and the OS kernel. When your C code calls `read()`, `write()`, `fork()`, `open()`, `socket()`, etc., these are system calls that:
1. Switch CPU from **user mode** to **kernel mode**
2. Execute privileged kernel code
3. Return result back to user mode

### System Calls Used in This Project

| System Call | Header | Where Used |
|------------|--------|-----------|
| `fork()` | `<unistd.h>` | `server.c` `main()` |
| `read()` | `<unistd.h>` | Throughout all modules |
| `write()` | `<unistd.h>` | Throughout all modules |
| `open()` | `<fcntl.h>` | All file operations |
| `close()` | `<unistd.h>` | All file operations |
| `lseek()` | `<unistd.h>` | `Customer.h`, `Employee.h`, `Manager.h`, `Admin.h` |
| `fcntl()` | `<fcntl.h>` | File locking throughout |
| `socket()` | `<sys/socket.h>` | `server.c`, `client.c` |
| `bind()` | `<sys/socket.h>` | `server.c` |
| `listen()` | `<sys/socket.h>` | `server.c` |
| `accept()` | `<sys/socket.h>` | `server.c` |
| `connect()` | `<sys/socket.h>` | `client.c` |
| `getpid()` | `<unistd.h>` | File locking (`struct flock`) |
| `fsync()` | `<unistd.h>` | `Customer.h` `withdrawMoney()`, `transferFunds()` |
| `ftruncate()` | `<unistd.h>` | `Customer.h` `applyLoan()` |
| `unlink()` | `<unistd.h>` | Semaphore cleanup, `/dev/shm` cleanup |

### Context Switching (Interview Level)
When a system call is made:
1. **Trap/Interrupt** — CPU switches to kernel mode via a software interrupt
2. **Kernel executes** the requested operation with full privileges
3. **Return** — CPU switches back to user mode with the result

The process that made the system call is blocked during this time. If the system call can block (e.g., `read()` waiting for data), the scheduler runs other processes while this one waits.

---

## 6. Process Scheduling (Conceptual)

### How Multiple Client Processes Get CPU Time
The Linux scheduler (CFS — Completely Fair Scheduler) allocates CPU time across all running processes:
- Each child process handling a client gets a fair share of CPU
- When a child is blocked on `read()` (waiting for client input), it sleeps — no CPU wasted
- When data arrives on the socket, the OS wakes the child and the scheduler gives it CPU time

### Why This Matters for This Project
Banking operations are **I/O-bound**, not CPU-bound:
- Most of the time, child processes are blocked waiting for client input (`read()`)
- This means dozens of concurrent clients can be handled efficiently
- The CPU is freed up for other processes while clients are thinking/typing

---

## 7. Zombie Processes — A Known Issue

### What is a Zombie?
When a child process exits, it becomes a **zombie** until the parent calls `wait()` or `waitpid()`. A zombie holds its entry in the process table but no memory.

### Does This Project Handle It?
The server's parent loop **never calls `wait()`** for child processes. This means every child that exits becomes a zombie until the server itself exits.

**Fix:** Install a `SIGCHLD` handler that calls `waitpid()`:
```c
// Not in project — this is the fix
signal(SIGCHLD, SIG_IGN);  // Simplest: tell OS to auto-reap children
// OR
void sigchld_handler(int s) { while(waitpid(-1, NULL, WNOHANG) > 0); }
signal(SIGCHLD, sigchld_handler);
```

**Interview answer:** "The project does not explicitly handle zombie processes. In a production system, I would add a SIGCHLD handler with `waitpid(WNOHANG)` to reap children automatically."

---

## 8. Common Interview Questions

**Q1: Why use fork() instead of threads for handling multiple clients?**
> Process isolation: if one client's handler crashes (e.g., SIGSEGV), only that child process dies. The parent continues accepting clients. With threads, a crash in any thread kills the entire server — all clients lose their sessions. The trade-off is that fork() is slower and heavier than thread creation, but for a banking system, safety > performance.

**Q2: What is the difference between a process and a thread?**
> A process has its own memory address space, file descriptors, and PID. Threads within the same process share the address space and file descriptors but have their own stack and registers. Threads are lighter (faster to create, less memory) but less isolated. Processes are heavier but safer.

**Q3: What does fork() return?**
> Returns 0 to the child process, the child's PID (a positive integer) to the parent, and -1 on failure (e.g., out of memory or ulimit on processes reached).

**Q4: What happens to open file descriptors after fork()?**
> The child inherits copies of all the parent's open file descriptors. They point to the same underlying file descriptions (same offset, same flags). The parent should close the connection fd, and the child should close the listening fd — both have reference counts, and a fd is truly closed only when all references are closed.

**Q5: What is a zombie process and how do you prevent it?**
> A zombie is a process that has exited but whose exit status hasn't been collected by the parent. It wastes a process table entry. Prevent it by calling `wait()` or `waitpid()` in the parent, or by setting `signal(SIGCHLD, SIG_IGN)` to let the kernel auto-reap children.

**Q6: What is Copy-on-Write in the context of fork()?**
> After fork(), the child shares the same physical memory pages as the parent. The OS marks all pages as read-only. When either process writes to a page, the OS creates a private copy of that page for the writing process. This makes fork() fast — no immediate memory copy is needed.

**Q7: How many concurrent clients can this server handle?**
> Theoretically, limited by: (1) the OS per-process fd limit (default 1024), (2) the number of processes allowed (ulimit -u), and (3) available memory. The listen backlog (2) only limits pending connections, not active ones.

---

## 9. Follow-up Questions

> **Q: Why not use `exec()` in the child process?**
> The child process doesn't need to run a different program — it runs `connectionHandler()` which is already in the same binary. `exec()` is used when you want the child to run a completely different program (e.g., a shell running a command).

> **Q: What is `_exit()` vs `exit()`?**
> `exit()` flushes stdio buffers and calls `atexit()` handlers before exiting. `_exit()` terminates immediately without cleanup. In `cleanupSemaphore()` in this project, `_exit(signum)` is used because we're in a signal handler — calling `exit()` from a signal handler can cause undefined behavior if the signal interrupted a stdio function.

> **Q: What is the process group and why does it matter for signals?**
> After fork(), the child is in the same process group as the parent. Pressing Ctrl+C sends SIGINT to the entire process group. That's why `setupSignalHandlers()` is called in the child — to handle SIGINT and cleanly release the semaphore before dying.

---

## 10. Common Mistakes in Interviews

- Saying `fork()` creates a **thread** — fork creates a **process** (separate address space)
- Forgetting that `fork()` returns **twice** — once in parent, once in child
- Saying shared memory is automatically available after fork() — it's NOT (unless explicitly set up with `shmget`/`mmap`)
- Ignoring zombie processes — always mention `waitpid()` or `SIGCHLD` handling
- Confusing fork() failure (`-1`) with the child process return value (`0`)

---

## 📌 Revision Box — One-Liners

| Concept | One-Line Explanation |
|---------|---------------------|
| `fork()` | Clones the current process; returns 0 to child, child PID to parent |
| Process isolation | Each child's crash doesn't affect the parent or other children |
| File descriptor inheritance | Child gets copies of all parent's open fds after fork() |
| Copy-on-Write | Memory pages shared until written — makes fork() fast |
| Zombie process | Exited child whose status hasn't been collected by parent via wait() |
| System call | Controlled entry into kernel mode to access OS services |
| Context switch | OS saves current process state, restores another process's state |
| I/O-bound process | Mostly blocked on I/O (read/write) — not consuming CPU |
| `_exit()` vs `exit()` | `_exit()` = immediate; `exit()` = flush buffers + atexit handlers |

**Frequently Confused:**
- fork() vs exec() — fork creates a copy of the process; exec replaces the process image
- Threads vs processes — threads share memory; processes don't (unless explicitly shared)
- The child process running `connectionHandler()` does NOT affect the parent's variables
