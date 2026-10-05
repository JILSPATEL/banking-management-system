# ⭐⭐ Medium Priority — Signals, Password Hashing, Terminal Control, Error Handling
> **Priority: MEDIUM** — Important supporting concepts with moderate interview frequency

---

## 1. Signal Handling — SIGINT, SIGTERM, SIGSEGV, SIGHUP, SIGQUIT

### Concept Overview
A **signal** is an asynchronous notification sent to a process. The OS or another process sends signals to notify events like: user pressed Ctrl+C, process crashed, terminal closed, etc.

### Why Signals are Used in This Project
Named semaphores persist in `/dev/shm/` even after a process dies. If a client process crashes (or is killed) while holding a semaphore, the semaphore stays locked — permanently preventing that account from logging in.

**Solution:** Register signal handlers that release the semaphore before the process terminates.

### Project Mapping

| Signal | Meaning | Handler in Project |
|--------|---------|-------------------|
| `SIGINT` | Ctrl+C pressed | `cleanupSemaphore()` — releases semaphore, exits |
| `SIGTERM` | Kill request (polite) | `cleanupSemaphore()` |
| `SIGSEGV` | Segmentation fault (crash) | `cleanupSemaphore()` |
| `SIGHUP` | Terminal/parent disconnected | `cleanupSemaphore()` |
| `SIGQUIT` | Ctrl+\\ pressed | `cleanupSemaphore()` |

### Where Registered

| File | Function | Call |
|------|----------|------|
| `server.c` | `setupSignalHandlers()` | `signal(SIGINT, cleanupSemaphore)` x5 |
| `Employee.h` | `loginEmployee()` | `setupSignalHandlers()` called after acquiring semaphore |
| `Manager.h` | `loginManager()` | `setupSignalHandlers()` called after acquiring semaphore |

### The cleanupSemaphore() Function

```
cleanupSemaphore(signum) in server.c:
  sem_post(sema)       → Release the semaphore (allow re-login)
  sem_close(sema)      → Close process's handle to it
  sem_unlink(semName)  → Delete from /dev/shm
  _exit(signum)        → Terminate immediately (no stdio flush)
```

### Why _exit() Instead of exit()?
Signal handlers should use `_exit()` rather than `exit()` because:
- `exit()` calls `atexit()` handlers and flushes stdio buffers
- If the signal interrupted a stdio function, calling `exit()` from the signal handler can cause **undefined behavior** (deadlock or double-free)
- `_exit()` terminates immediately — no cleanup, no buffer flush

### signal() vs sigaction()
The project uses `signal()` (simpler but less portable). `sigaction()` is the POSIX-preferred alternative:
- `signal()` behavior differs across Unix variants (re-registration, SA_RESTART)
- `sigaction()` gives explicit control over flags like `SA_RESTART` (restart interrupted system calls) and `SA_NODEFER` (don't block signal during handler)

### Common Interview Questions — Signals

**Q: What signals cannot be caught or ignored?**
> `SIGKILL` (9) and `SIGSTOP` cannot be caught, blocked, or ignored. SIGKILL immediately terminates a process. SIGSTOP suspends it. This is why `kill -9` always works.

**Q: What is the difference between SIGTERM and SIGKILL?**
> SIGTERM is a polite termination request — the process can catch it, clean up, and exit gracefully. SIGKILL is unconditional — the kernel immediately terminates the process without running any handlers.

**Q: Why does this project catch SIGSEGV?**
> SIGSEGV is a segmentation fault — usually caused by a null pointer dereference or buffer overflow. Normally it crashes the process. Here, catching it allows the semaphore to be released before the process dies. However, calling non-async-signal-safe functions in a SIGSEGV handler is technically undefined behavior.

**Q: What is a signal-safe function?**
> A function that can safely be called from a signal handler. These are async-signal-safe functions listed in POSIX (e.g., `_exit()`, `write()`, `sem_post()`). `printf()`, `malloc()`, and most stdio functions are NOT signal-safe.

---

## 2. Password Hashing — crypt() and SHA-512

### Concept Overview
Passwords must never be stored in plaintext. This project uses `crypt()` to hash passwords before storing them.

### Project Mapping

| Operation | File | Function | Call |
|-----------|------|----------|------|
| Hash on registration | `Employee.h` | `addCustomer()` | `crypt(readBuffer, HASHKEY)` |
| Hash on registration | `Admin.h` | `addEmployee()` | `crypt(readBuffer, HASHKEY)` |
| Hash on login | `Customer.h` | `loginCustomer()` | `crypt(password, HASHKEY)` |
| Hash on login | `Employee.h` | `loginEmployee()` | `crypt(password, HASHKEY)` |
| Hash on password change | `Customer.h` | `changePassword()` | `crypt(newPassword, HASHKEY)` |
| Hash on password change | `Employee.h` | `changeEMPPassword()` | `crypt(newPassword, HASHKEY)` |

### The HASHKEY
```c
#define HASHKEY "$6$saltsalt$"
```
- `$6$` — Specifies **SHA-512** hashing algorithm (crypt() format)
- `saltsalt` — The salt value
- Other options: `$1$`=MD5, `$2$`=bcrypt, `$5$`=SHA-256

### How It Works
```
Password:  "mypassword123"
Salt:      "$6$saltsalt$"
↓ SHA-512 hash
Output:    "$6$saltsalt$[86-character hash]..."
Stored in: customer.password[256]
```

On login, the input password is hashed with the **same salt** and compared against the stored hash. If they match, login succeeds.

### Why a Fixed Salt is a Security Issue
A **fixed salt** means two users with the same password get the same hash. An attacker with the database can:
1. Build a **rainbow table** for this specific salt
2. Crack all passwords at once

**Better practice:** Generate a unique random salt per user (e.g., using `/dev/urandom`) and store it alongside the hash.

### Compilation Requirement
```makefile
gcc server.c -o server -pthread -lcrypt -lrt
```
`-lcrypt` links the crypt library. Without it, `crypt()` is undefined.

### Common Interview Questions — Password Hashing

**Q: Why not store passwords in plaintext?**
> If the database/files are compromised, plaintext passwords expose all accounts immediately. Hashes are one-way — given the hash, you cannot recover the original password (without brute force). Even the system admin cannot see user passwords.

**Q: What is the difference between hashing and encryption?**
> Hashing is one-way (irreversible), deterministic, and fixed-output-size. Encryption is two-way (reversible with a key). Passwords should be hashed, not encrypted — you never need to recover the original password, only verify it.

**Q: What is a salt and why is it needed?**
> A salt is random data added to the password before hashing. Without a salt, two users with the same password have identical hashes — an attacker can build a rainbow table (precomputed hash→password map) to crack all of them at once. A unique salt per user makes rainbow tables impractical.

---

## 3. Terminal Control — termios and Password Hiding

### Concept Overview
`termios` is a POSIX API for controlling terminal behavior — line discipline, echo, canonical mode, etc.

### Why Used in This Project
When a user types a password, the characters should not be visible on screen (no echo). The `hide_input()` function in `client.c` disables terminal echo to hide password input.

### Project Mapping

| File | Function | Purpose |
|------|----------|---------|
| `client.c` | `hide_input()` | Disable ECHO, read password, restore ECHO |

### How hide_input() Works
```
1. tcgetattr(STDIN_FILENO, &oldt)     → Save current terminal settings
2. newt = oldt                         → Copy settings
3. newt.c_lflag &= ~ECHO              → Turn off ECHO bit in local flags
4. tcsetattr(STDIN_FILENO, TCSANOW, &newt) → Apply immediately (TCSANOW)
5. fgets(buffer, size, stdin)          → Read password (not displayed)
6. tcsetattr(STDIN_FILENO, TCSANOW, &oldt) → Restore original settings
```

### Key termios Flags

| Flag | Meaning |
|------|---------|
| `ECHO` | Echo input characters to terminal |
| `TCSANOW` | Apply changes immediately |
| `TCSADRAIN` | Apply after all output is written |
| `c_lflag` | Local (line discipline) flags field |

### How the Server Signals the Client to Hide Input
The server sends the exact string `"Enter password: "` when it wants hidden input:
```c
// server.c (Customer.h)
strcpy(writeBuffer, "Enter password: ");
write(connectionFD, writeBuffer, ...);
```

The client checks for this exact string:
```c
// client.c connectionHandler()
if (strcmp(readBuffer, "Enter password: ") == 0) {
    hide_input(writeBuffer, sizeof(writeBuffer));
}
```

This is a **custom application-layer protocol** — the `"Enter password: "` string acts as a signal to switch to hidden input mode.

### Common Interview Questions — Terminal Control

**Q: Why is ECHO a terminal property, not a program property?**
> The terminal (line discipline) is a kernel subsystem that processes input before delivering it to the program. Echo is handled by the kernel before the program even sees the keystrokes. The program must use termios to change this behavior.

**Q: What is TCSANOW vs TCSADRAIN?**
> TCSANOW applies the terminal settings immediately. TCSADRAIN waits until all output buffered for the terminal has been written before applying. TCSAFLUSH does the same but also discards pending input. TCSANOW is used here because we want instant effect before the user starts typing.

---

## 4. Error Handling — perror(), errno, and Return Codes

### Concept Overview
System calls return -1 on failure and set the global variable `errno` to indicate the type of error. `perror()` prints a human-readable error message corresponding to the current `errno`.

### Project-Wide Error Handling Pattern

```c
int file = open(CUSPATH, O_RDWR, 0644);
if (file == -1) {
    printf("Error opening file!\n");
    return;   // or exit(-1)
}
```

### errno Values Used in This Project

| errno Value | Constant | Where Used |
|------------|----------|-----------|
| Would block | `EAGAIN` | `sem_trywait()` — semaphore is locked (duplicate login) |
| Other errors | Various | `perror()` prints them |

### perror() vs strerror()
- `perror(prefix)` — prints `prefix: <error message>` to stderr using current errno
- `strerror(errno)` — returns the error string for use in custom messages
- This project primarily uses `perror()` for socket/file errors and `printf()` for business logic errors

### Error Handling Gaps
The project does not always check return values:
```c
write(connectionFD, writeBuffer, sizeof(writeBuffer));  // Return not checked
read(connectionFD, readBuffer, sizeof(readBuffer));     // Return not checked
```

**Interview note:** "In production, every system call return value should be checked. Silent failures in banking software are unacceptable. I would add proper return value checking and logging."

### Common Interview Questions — Error Handling

**Q: What is errno and is it thread-safe?**
> `errno` is a global (or thread-local) variable set by system calls on failure. In multithreaded programs, errno is thread-local (each thread has its own errno). In multiprocess programs (like this one), each process has its own errno.

**Q: What does EAGAIN mean?**
> "Resource temporarily unavailable." For sem_trywait(), it means the semaphore value is 0 (locked). For non-blocking sockets, it means no data is currently available. It tells you to try again later — the operation can succeed in the future.

---

## 5. Directory Traversal — opendir/readdir for /dev/shm Cleanup

### Concept Overview
`opendir()`, `readdir()`, `closedir()` are POSIX functions for listing directory contents. Used in this project for semaphore cleanup on startup.

### Project Mapping

| File | Function | Purpose |
|------|----------|---------|
| `server.c` | `cleanupAllSemaphores()` | Scan `/dev/shm/` and delete stale `sem.sem_*` files |

### How cleanupAllSemaphores() Works
```
1. opendir("/dev/shm")        → Open the directory
2. readdir(d) in a loop       → Read each entry (struct dirent)
3. strncmp(name, "sem.sem_")  → Check if it's a project semaphore
4. snprintf(path, ...)        → Build full path (/dev/shm/sem.sem_1001)
5. unlink(path)               → Delete the file
6. closedir(d)                → Close directory
```

### Why This is Needed
If the server crashes (killed with SIGKILL, power loss), signal handlers don't run. Semaphores remain in `/dev/shm/`. On next server start, accounts would appear "already logged in" with no way to clear them. `cleanupAllSemaphores()` runs at startup to clear this state.

### Common Interview Questions — Directory Traversal

**Q: What is struct dirent?**
> It's the structure returned by readdir(). Contains `d_name` (filename as a string) and `d_type` (file type: regular file, directory, symlink, etc.).

**Q: What is the difference between unlink() and remove()?**
> `unlink()` removes a file by name (POSIX system call). `remove()` is a C standard library function that calls `unlink()` for files and `rmdir()` for directories.

---

## 📌 Revision Box — One-Liners

| Concept | One-Line Explanation |
|---------|---------------------|
| Signal | Asynchronous notification sent to a process by OS or another process |
| `SIGINT` | Ctrl+C — sent to foreground process group |
| `SIGKILL` | Cannot be caught or ignored — immediate process termination |
| `_exit()` | Terminate immediately without flushing stdio — safe in signal handlers |
| `crypt()` | POSIX function for one-way password hashing using DES/MD5/SHA |
| `$6$` prefix | Signals SHA-512 hashing in crypt() format |
| Salt | Random data mixed with password before hashing to defeat rainbow tables |
| `termios` | POSIX API for controlling terminal input/output behavior |
| `ECHO` flag | Controls whether typed characters are displayed back on the terminal |
| `TCSANOW` | Apply terminal settings immediately |
| `errno` | Global variable set by system calls to indicate error type |
| `EAGAIN` | Temporary failure — resource busy, try again (sem_trywait, non-blocking I/O) |
| `perror()` | Prints error message string corresponding to current errno |
| `opendir/readdir` | POSIX API for listing directory contents |

**Frequently Confused:**
- Hashing vs encryption — hashing is one-way; encryption is reversible
- `signal()` vs `sigaction()` — sigaction is preferred; signal() has undefined behavior on some platforms
- `SIGTERM` (catchable) vs `SIGKILL` (not catchable) — always mention this pair
