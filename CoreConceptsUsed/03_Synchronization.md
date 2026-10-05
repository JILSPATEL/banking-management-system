# ⭐⭐⭐ Synchronization — Semaphores, File Locking, Race Conditions
> **Priority: HIGH** — Concurrent access, race conditions, and locking are top interview topics

---

## 1. The Core Problem — Race Conditions in Banking

### Why Synchronization is Critical
Multiple clients can be logged in simultaneously. Without synchronization:

**Scenario (Race Condition):**
```
Client A reads balance: ₹1000
Client B reads balance: ₹1000
Client A subtracts ₹500, writes ₹500
Client B subtracts ₹800, writes ₹200  ← WRONG! Should have failed (insufficient funds)
Result: Bank loses ₹300
```

This project uses **two layers** of synchronization:
1. **POSIX Named Semaphores** — Prevent duplicate logins (session-level)
2. **fcntl File Locking** — Prevent concurrent file record updates (data-level)

---

## 2. POSIX Named Semaphores — Duplicate Login Prevention

### Concept Overview
A **semaphore** is a synchronization primitive — an integer that supports two atomic operations:
- **wait (P / down / sem_wait)** — Decrement; if result < 0, block
- **signal (V / up / sem_post)** — Increment; wake one blocked waiter

A **binary semaphore** (initialized to 1) acts like a **mutex** — only one process can hold it at a time.

A **POSIX Named Semaphore** is identified by a name (e.g., `/sem_1001`) and lives in `/dev/shm/`. It can be shared across **multiple unrelated processes** — exactly what fork() creates.

### Why Named Semaphores (Not Anonymous)?
After `fork()`, parent and child are separate processes. Anonymous semaphores in shared memory would work too, but named semaphores are simpler — you access them by name from any process without needing to set up shared memory explicitly.

### Project Mapping

| Operation | File | Function | Purpose |
|-----------|------|----------|---------|
| Create/open semaphore | `server.c` | `initializeSemaphore(id)` | `sem_open("/sem_<id>", O_CREAT, 0644, 1)` |
| Try to acquire (non-blocking) | `Customer.h` | `loginCustomer()` | `sem_trywait(cust_sema)` |
| Try to acquire (non-blocking) | `Employee.h` | `loginEmployee()` | `sem_trywait(sema)` |
| Try to acquire (non-blocking) | `Manager.h` | `loginManager()` | `sem_trywait(sema)` |
| Release semaphore | `Customer.h` | `logout()` | `sem_post()` |
| Release semaphore | `server.c` | `cleanupSemaphore()` | `sem_post()` (on signal) |
| Close handle | Multiple | logout/exitClient | `sem_close()` |
| Delete semaphore | Multiple | logout/exitClient | `sem_unlink()` |
| Startup cleanup | `server.c` | `cleanupAllSemaphores()` | Removes stale `/dev/shm/sem.sem_*` files |

### How the Duplicate Login Prevention Works

```
Customer 1001 logs in on Terminal A:
  sem_open("/sem_1001", O_CREAT, 0644, 1)  → semaphore created, value = 1
  sem_trywait("/sem_1001")                  → value becomes 0 (locked)
  Login proceeds normally

Customer 1001 tries to log in on Terminal B:
  sem_open("/sem_1001", O_CREAT, 0644, 1)  → O_CREAT ignored, existing semaphore returned
  sem_trywait("/sem_1001")                  → value is 0, EAGAIN returned
  errno == EAGAIN → "You are already logged in on another terminal"
  Returns 3 → client sees rejection message

Customer 1001 logs out on Terminal A:
  sem_post("/sem_1001")   → value back to 1
  sem_close()             → close this process's handle
  sem_unlink("/sem_1001") → delete from /dev/shm
```

### Internal Working of sem_trywait vs sem_wait

| Function | Behavior when value = 0 |
|----------|------------------------|
| `sem_wait()` | Blocks (sleeps) until value > 0 |
| `sem_trywait()` | Returns immediately with -1, errno=EAGAIN |

`sem_trywait()` is used here because we want to **detect** a duplicate login, not **wait** for the user to log out.

### /dev/shm — Where Named Semaphores Live
Named semaphores on Linux are stored as files in `/dev/shm/` with the prefix `sem.`:
- `/sem_1001` → stored as `/dev/shm/sem.sem_1001`
- This is a **tmpfs** filesystem — lives in RAM, not on disk
- `cleanupAllSemaphores()` in `server.c` scans `/dev/shm/` on startup and removes stale semaphore files from a previous crashed run

---

## 3. POSIX Semaphore API — Detailed Reference

| Function | Signature | Purpose |
|----------|-----------|---------|
| `sem_open()` | `sem_t* sem_open(name, flags, mode, value)` | Create/open named semaphore |
| `sem_wait()` | `int sem_wait(sem_t*)` | Decrement; block if 0 |
| `sem_trywait()` | `int sem_trywait(sem_t*)` | Decrement; fail immediately if 0 |
| `sem_post()` | `int sem_post(sem_t*)` | Increment; wake waiter |
| `sem_close()` | `int sem_close(sem_t*)` | Close this process's handle |
| `sem_unlink()` | `int sem_unlink(const char*)` | Delete the semaphore (by name) |

**`sem_close()` vs `sem_unlink()`:**
- `sem_close()` — releases this process's handle. Other processes can still use it.
- `sem_unlink()` — marks the semaphore for deletion. It's actually deleted when all processes have `sem_close()`d it.

---

## 4. fcntl File Locking — Protecting Individual Records

### Concept Overview
`fcntl()` provides **advisory file locking** on Linux. It allows processes to lock byte ranges within a file. "Advisory" means the OS does not enforce it — processes must cooperate by calling `fcntl()` before accessing shared data.

### Why Record-Level Locking?
Banking files store binary records sequentially. For `customers.txt`:
```
[Customer 1001 record][Customer 1002 record][Customer 1003 record]...
offset 0               offset N              offset 2N
```

When Customer 1001 is depositing money, **only their record** needs to be locked. Customer 1002 can still withdraw simultaneously. This is **fine-grained locking** — better concurrency than locking the entire file.

### Project Mapping

| Operation | File | Function | Locked Region |
|-----------|------|----------|--------------|
| Deposit | `Customer.h` | `depositMoney()` | Customer record (sizeof struct) |
| Withdraw | `Customer.h` | `withdrawMoney()` | Customer record (sizeof struct) |
| Transfer | `Customer.h` | `transferFunds()` | Entire customer file (l_len=0) |
| Change password (customer) | `Customer.h` | `changePassword()` | Customer record |
| Change password (employee) | `Employee.h` | `changeEMPPassword()` | Employee record |
| Change password (manager) | `Manager.h` | `changeMNGPassword()` | Manager record |
| Approve/reject loan | `Employee.h` | `approveRejectLoan()` | Loan record + Customer record |
| Assign loan to employee | `Manager.h` | `assignLoanApplication()` | Loan record |
| Modify customer/employee | `Admin.h` | `modifyCE()` | Customer or Employee record |

### struct flock — The Lock Descriptor

```c
struct flock {
    short  l_type;    // F_RDLCK, F_WRLCK, F_UNLCK
    short  l_whence;  // SEEK_SET, SEEK_CUR, SEEK_END
    off_t  l_start;   // Offset from l_whence
    off_t  l_len;     // Length of region (0 = to EOF)
    pid_t  l_pid;     // PID of process holding lock (output only)
};
```

### F_SETLK vs F_SETLKW

| Command | Behavior when lock is held by another process |
|---------|----------------------------------------------|
| `F_SETLK` | Returns -1 immediately (errno=EACCES or EAGAIN) |
| `F_SETLKW` | Blocks until lock is available (**W** = Wait) |

**Where each is used in this project:**
- `F_SETLKW` — `depositMoney()`, `withdrawMoney()`, `changePassword()` — blocking wait is acceptable for financial ops
- `F_SETLK` — `approveRejectLoan()`, `assignLoanApplication()` — tries to lock; if already locked, reports "already processed"

### transferFunds() — Whole-File Lock for Atomicity

`transferFunds()` locks the entire customer file (`l_len = 0`) to prevent:
- A concurrent transfer from the same source account
- A concurrent deposit to the destination account that could interfere

This is a **coarser** lock but ensures **atomicity** of the two-account update.

### The Locking Sequence Pattern Used Throughout

```c
// 1. Seek to find the record
lseek(file, 0, SEEK_SET);
while (read(file, &record, sizeof(record)) == sizeof(record)) {
    if (record.id == targetId) {
        offset = lseek(file, -sizeof(record), SEEK_CUR);  // Save offset
        break;
    }
}

// 2. Set write lock on that record
struct flock fl = {F_WRLCK, SEEK_SET, offset, sizeof(record), getpid()};
fcntl(file, F_SETLKW, &fl);  // Block until locked

// 3. Seek back, re-read (fresh data), modify, write back
lseek(file, offset, SEEK_SET);
read(file, &record, sizeof(record));
record.balance += depositAmount;
lseek(file, offset, SEEK_SET);  // Actually: seek back after lock-then-reread
write(file, &record, sizeof(record));

// 4. Release lock
fl.l_type = F_UNLCK;
fcntl(file, F_SETLK, &fl);
```

**Why re-read after locking?** The data could have changed between the first read (to find the offset) and when the lock is acquired. Re-reading gets the most current data.

---

## 5. Critical Section

### Concept Overview
A **critical section** is a region of code that accesses shared resources and must not be executed by more than one process/thread at a time.

### Critical Sections in This Project

| Critical Section | Shared Resource | Protection Used |
|-----------------|-----------------|----------------|
| Deposit/Withdraw balance update | `customers.txt` record | `fcntl` write lock |
| Transfer (two accounts) | `customers.txt` file | `fcntl` whole-file write lock |
| Loan approval (loan + customer update) | `loanDetails.txt` + `customers.txt` | Two separate fcntl locks |
| Login (check + acquire session) | Named semaphore | `sem_trywait()` (atomic) |
| Logout (release session) | Named semaphore | `sem_post()` |

---

## 6. Semaphore vs Mutex vs File Lock — Comparison

| Feature | POSIX Named Semaphore | Mutex | fcntl File Lock |
|---------|----------------------|-------|----------------|
| Scope | Cross-process | Within a process | Cross-process |
| Blocking | Yes (sem_wait) | Yes (pthread_mutex_lock) | Yes (F_SETLKW) |
| Non-blocking | Yes (sem_trywait) | Yes (trylock) | Yes (F_SETLK) |
| Use in this project | Session (login) management | Not used | Record-level data protection |
| Location | /dev/shm | Thread's address space | Kernel file lock table |
| Persistence | Until unlinked or reboot | Process lifetime | Released on close/exit |

---

## 7. Common Interview Questions

**Q1: What is a race condition? Give an example from your project.**
> A race condition occurs when the outcome depends on the sequence of events that are not controlled. In this project, if two processes both read the same customer balance (₹1000) before either writes back, then both subtract amounts and write back — one update is lost. Prevented with fcntl write locks.

**Q2: What is the difference between a semaphore and a mutex?**
> A mutex is a locking primitive that can only be released by the thread/process that acquired it. A semaphore has no ownership — any process can call sem_post, even if it didn't call sem_wait. In this project, semaphores are used for session management (one user per account), not strict mutual exclusion of code.

**Q3: What is the difference between sem_wait and sem_trywait?**
> sem_wait blocks if the semaphore value is 0, sleeping until another process calls sem_post. sem_trywait returns immediately with errno=EAGAIN if the value is 0. This project uses sem_trywait for duplicate login detection — we want to know immediately if the account is already in use.

**Q4: What is advisory locking? Why doesn't Linux enforce it?**
> Advisory locking means the OS doesn't prevent uncooperating processes from accessing locked regions. All processes must agree to check and respect the lock. Linux uses advisory locking because enforcing it for all file accesses would add overhead. Mandatory locking exists (with specific mount options) but is deprecated in Linux.

**Q5: What is a deadlock and can it happen in this project?**
> A deadlock occurs when two processes each hold a resource the other needs, and both are waiting — neither can proceed. In `approveRejectLoan()`, the employee locks the loan record, then tries to lock the customer record. If another operation held the customer lock and tried to acquire the loan lock simultaneously, deadlock could occur. The project mitigates this partially by using `F_SETLK` (non-blocking) for the loan record first.

**Q6: What is sem_close vs sem_unlink?**
> sem_close releases the calling process's handle to the semaphore — like closing a file. The semaphore itself still exists. sem_unlink marks the semaphore for deletion. It's fully deleted once all processes have called sem_close.

**Q7: Why are semaphores stored in /dev/shm?**
> Named POSIX semaphores on Linux are implemented as files in /dev/shm, which is a tmpfs (RAM-backed) filesystem. This gives fast in-memory access while providing a namespace for processes to find them by name. The file is prefixed with `sem.`.

---

## 8. Follow-up Questions

> **Q: Why use two locking mechanisms (semaphore + fcntl)?**
> They solve different problems. Semaphores prevent the same account from being used in two simultaneous sessions (session-level). fcntl prevents concurrent writes to the same file record (data-level). You need both: semaphores alone don't prevent two different processes from modifying the file even in the same session, and file locks alone don't prevent two login sessions for the same account.

> **Q: What happens to locks when a process crashes?**
> **fcntl locks** are automatically released by the kernel when the process dies or closes the file — they are not persistent. **Named semaphores** are NOT automatically released — if a process crashes while holding the semaphore, it stays locked. That's why `cleanupSemaphore()` is registered as a signal handler (SIGINT, SIGTERM, SIGSEGV, SIGHUP, SIGQUIT) — to call `sem_post()` before dying.

> **Q: What if SIGKILL is sent to the server?**
> SIGKILL cannot be caught or ignored. The signal handler won't run. Named semaphores will remain locked in /dev/shm. That's why `cleanupAllSemaphores()` is called at server startup — to clean up any stale semaphores from a previous crashed run.

---

## 9. Common Mistakes in Interviews

- Confusing semaphores with locks — semaphores are counters; they can count beyond 1 (counting semaphores)
- Forgetting `sem_unlink()` — sem_close alone doesn't delete the semaphore from /dev/shm
- Saying fcntl locks prevent all access — they are advisory; a process that doesn't call fcntl can still read/write
- Ignoring the "re-read after lock" pattern — reading before locking gives stale data
- Confusing F_SETLK (non-blocking) with F_SETLKW (blocking with wait)

---

## 📌 Revision Box — One-Liners

| Concept | One-Line Explanation |
|---------|---------------------|
| Race condition | Two processes read-modify-write shared data without coordination |
| Critical section | Code region that must execute atomically with respect to shared resources |
| Binary semaphore | Semaphore initialized to 1; acts like a mutex but without ownership |
| Named semaphore | POSIX semaphore identified by a name; shared across unrelated processes via /dev/shm |
| `sem_trywait()` | Non-blocking semaphore decrement; returns EAGAIN if already locked |
| `sem_post()` | Increments semaphore; releases the lock; wakes a waiting process |
| `fcntl F_SETLKW` | Advisory write lock; blocks until lock is available |
| `fcntl F_SETLK` | Advisory write lock; fails immediately if lock is held |
| `l_len = 0` | Lock from l_start to end-of-file (whole file lock) |
| Advisory lock | Lock the OS records but does NOT enforce on uncooperating processes |

**Frequently Confused:**
- `sem_wait` (blocking) vs `sem_trywait` (non-blocking) — this project uses `sem_trywait` specifically because blocking on a duplicate login makes no sense
- Named vs anonymous semaphores — named semaphores survive fork() because child finds them by name; anonymous ones in shared memory also work but require more setup
- fcntl locks vs flock() — `flock()` locks entire files; `fcntl()` can lock byte ranges (record-level)
