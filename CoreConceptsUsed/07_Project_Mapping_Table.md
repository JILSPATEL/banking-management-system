# 📋 Project Mapping Summary Table
> **Complete reference:** Every feature → Filename → Function → OS/Linux Concept → Why Used

---

## Master Feature-to-Concept Mapping

| Feature | Filename | Function Name(s) | OS/Linux Concept | Why Used |
|---------|----------|------------------|-----------------|----------|
| **SERVER SETUP** | | | | |
| Create server socket | `server.c` | `main()` | TCP Socket (`socket()`) | Entry point for all client connections |
| Allow port reuse | `server.c` | `main()` | `SO_REUSEADDR` / `setsockopt()` | Prevent "Address in use" on restart |
| Bind to port 8080 | `server.c` | `main()` | `bind()`, `htons()`, `INADDR_ANY` | Assign server address and port |
| Listen for connections | `server.c` | `main()` | `listen()`, TCP backlog | Mark socket as passive; queue pending connections |
| Accept client connections | `server.c` | `main()` | `accept()`, blocking I/O | Dequeue each client connection, get per-client fd |
| Handle multiple clients | `server.c` | `main()` | `fork()`, child processes | Each client gets an isolated process |
| Route client to module | `server.c` | `connectionHandler()` | `read()`/`write()` on socket fd | Receive menu choice, dispatch to role handler |
| **CLIENT SETUP** | | | | |
| Create client socket | `client.c` | `main()` | TCP Socket (`socket()`) | Endpoint for connecting to server |
| Connect to server | `client.c` | `main()` | `connect()`, `inet_addr()`, `htons()` | Initiate TCP connection to 127.0.0.1:8080 |
| Client I/O loop | `client.c` | `connectionHandler()` | `read()`/`write()` on socket, blocking I/O | Continuous read-prompt/write-response cycle |
| Hide password input | `client.c` | `hide_input()` | `termios`, `ECHO`, `tcgetattr()`/`tcsetattr()` | Prevent password from displaying on terminal |
| **CUSTOMER MODULE** | | | | |
| Customer login | `Customer.h` | `loginCustomer()` | POSIX named semaphore (`sem_open`, `sem_trywait`) | Detect and prevent duplicate logins per account |
| Read customer credentials | `Customer.h` | `loginCustomer()` | `open()`, `read()`, `lseek()` — binary file I/O | Sequential scan of customers.txt for matching account |
| Verify password | `Customer.h` | `loginCustomer()` | `crypt()` SHA-512 hashing | Compare stored hash with hash of input password |
| Deposit money | `Customer.h` | `depositMoney()` | `fcntl()` `F_SETLKW` record-level write lock | Prevent concurrent modification of same customer record |
| Deposit money | `Customer.h` | `depositMoney()` | `lseek(SEEK_CUR, -sizeof)` | Step back to overwrite the customer record in place |
| Log deposit transaction | `Customer.h` | `depositMoney()` | `O_APPEND`, `write()` to `trans_hist.txt` | Atomically append transaction record |
| Timestamp transaction | `Customer.h` | `depositMoney()` | `time()`, `localtime()`, `struct tm` | Record exact date and time of transaction |
| Withdraw money | `Customer.h` | `withdrawMoney()` | `fcntl()` `F_SETLKW` record-level write lock | Prevent race condition on balance update |
| Write-through durability | `Customer.h` | `withdrawMoney()` | `fsync()` | Force balance + history to disk before returning |
| View balance | `Customer.h` | `customerBal()` | `open(O_RDONLY)`, `read()`, `lseek()` | Read-only sequential scan — no lock needed (read-only) |
| Apply for loan | `Customer.h` | `applyLoan()` | `O_RDWR | O_CREAT`, `read()`/`write()` binary | Read/update loan counter; append new loan record |
| Loan counter integrity | `Customer.h` | `applyLoan()` | `ftruncate()` | Keep loanCounter.txt exactly one struct in size |
| Transfer funds | `Customer.h` | `transferFunds()` | `fcntl()` whole-file write lock (`l_len=0`) | Lock entire file to atomically update two accounts |
| Transfer durability | `Customer.h` | `transferFunds()` | `fsync()` on both file and history | Ensure both account changes reach disk |
| View transaction history | `Customer.h` | `transactionHistory()` | `open(O_RDONLY)`, `lseek(SEEK_SET)`, `read()` | Sequential scan of trans_hist.txt filtering by account |
| Add feedback | `Customer.h` | `addFeedback()` | `O_APPEND | O_CREAT`, binary struct write | Append feedback struct to feedback.txt |
| Change password | `Customer.h` | `changePassword()` | `fcntl()` `F_SETLKW`, `crypt()`, `write()` | Lock record, hash new password, overwrite in place |
| Re-login after password change | `Customer.h` | `customerMenu()` | `goto label1` | Jump back to credential prompt for re-authentication |
| Customer logout | `Customer.h` | `logout()` | `sem_post()`, `sem_close()`, `sem_unlink()` | Release semaphore — allow account to login elsewhere |
| **EMPLOYEE MODULE** | | | | |
| Employee login | `Employee.h` | `loginEmployee()` | POSIX named semaphore (`initializeSemaphore`, `sem_trywait`) | Prevent duplicate employee sessions |
| Setup crash cleanup | `Employee.h` | `loginEmployee()` | `setupSignalHandlers()`, `signal()` | Register SIGINT/SIGTERM/SIGSEGV handlers for semaphore cleanup |
| Add new customer | `Employee.h` | `addCustomer()` | `open(O_APPEND | O_CREAT)`, binary struct write | Append new customer record + initial transaction history |
| Hash new customer password | `Employee.h` | `addCustomer()` | `crypt()` SHA-512 | Store password as secure hash, never plaintext |
| Approve/reject loan | `Employee.h` | `approveRejectLoan()` | `fcntl()` `F_SETLK` (non-blocking) on loan + `F_SETLKW` on customer | Lock both records; detect already-processed loans |
| View assigned loans | `Employee.h` | `viewAssignedLoan()` | `open(O_RDONLY)`, `read()` sequential scan | Filter loans by empID and status=pending |
| View customer transactions | `Employee.h` | `employeeMenu()` → `transactionHistory()` | File I/O on trans_hist.txt | Reuses customer transaction history function |
| Change employee password | `Employee.h` | `changeEMPPassword()` | `fcntl()` `F_SETLKW`, `crypt()`, write-back | Lock employee record, hash new password, overwrite |
| Employee logout | `Employee.h` | `employeeMenu()` → `logout()` | `sem_post()`, `sem_close()`, `sem_unlink()` | Release employee session semaphore |
| **MANAGER MODULE** | | | | |
| Manager login | `Manager.h` | `loginManager()` | POSIX named semaphore, `signal()` handlers | Prevent duplicate manager sessions |
| Activate/deactivate account | `Manager.h` | `changeStatus()` | `open()`, sequential scan, `lseek(SEEK_CUR, -sizeof)`, write | In-place update of `activeStatus` field in customer record |
| Assign loan to employee | `Manager.h` | `assignLoanApplication()` | `fcntl()` `F_SETLK` (non-blocking) write lock | Lock loan record; detect already-assigned loans |
| Review feedback | `Manager.h` | `readFeedBack()` | `open(O_RDONLY)`, binary read of FeedBack structs | Sequential scan of feedback.txt |
| Change manager password | `Manager.h` | `changeMNGPassword()` | `fcntl()` `F_SETLKW`, `crypt()`, write-back | Same pattern as employee password change |
| Manager logout | `Manager.h` | `managerMenu()` → `logout()` | `sem_post()`, `sem_close()`, `sem_unlink()` | Release manager session semaphore |
| **ADMIN MODULE** | | | | |
| Admin login | `Admin.h` | `adminMenu()` | Hardcoded string compare (`strcmp`) | Simple admin authentication (no semaphore — design limitation) |
| Add new employee | `Admin.h` | `addEmployee()` | `open(O_CREAT | O_RDWR)`, `lseek(SEEK_END)`, `write()` | Append new employee struct to employees.txt |
| Hash employee password | `Admin.h` | `addEmployee()` | `crypt()` SHA-512 | Store employee password as hash |
| Modify customer details | `Admin.h` | `modifyCE()` | `fcntl()` `F_SETLKW`, in-place name update | Lock customer record for safe in-place modification |
| Modify employee details | `Admin.h` | `modifyCE()` | `fcntl()` `F_SETLKW`, in-place name update | Lock employee record for safe in-place modification |
| Manage user roles | `Admin.h` | `manageRole()` | `lseek(SEEK_CUR, -sizeof)`, write back | In-place update of `role` field in employee record |
| Exit client | `Admin.h` / `server.c` | `exitClient()` | `sem_post()`, `write()` logout message | Release session and signal client to disconnect |
| **SERVER LIFECYCLE** | | | | |
| Semaphore cleanup on startup | `server.c` | `cleanupAllSemaphores()` | `opendir()`, `readdir()`, `unlink()` on `/dev/shm` | Remove stale semaphores from previous crashed run |
| Initialize session semaphore | `server.c` | `initializeSemaphore(id)` | `sem_open(O_CREAT, 0644, 1)` | Create named semaphore for any user type |
| Signal-based cleanup | `server.c` | `cleanupSemaphore()`, `setupSignalHandlers()` | `signal()`, SIGINT/SIGTERM/SIGSEGV/SIGHUP/SIGQUIT | Release semaphore on any termination signal |
| **DEBUG UTILITY** | | | | |
| Print all records | `debug.c` | `main()` | `fopen()`/`fread()`/`rewind()` — buffered FILE* I/O | Debug utility to inspect binary file contents |

---

## OS Concept Summary — How Many Times Each Appears

| OS/Linux Concept | Frequency | Files |
|-----------------|-----------|-------|
| `read()`/`write()` on socket | Every interaction | `server.c`, `client.c`, all Modules |
| `open()`/`read()`/`write()`/`close()` on files | Every data operation | All modules |
| `lseek()` | Every in-place update | `Customer.h`, `Employee.h`, `Manager.h`, `Admin.h` |
| `fcntl()` file locking | Every write operation | `Customer.h`, `Employee.h`, `Manager.h`, `Admin.h` |
| POSIX named semaphores | Every login/logout | `Customer.h`, `Employee.h`, `Manager.h`, `server.c` |
| `crypt()` password hashing | Registration + login + change | `Customer.h`, `Employee.h`, `Manager.h`, `Admin.h` |
| `fork()` | Once per client connection | `server.c` |
| `signal()` handlers | Login of employee/manager | `server.c`, `Employee.h`, `Manager.h` |
| `time()`/`localtime()` | Every transaction | `Customer.h`, `Employee.h` |
| `O_APPEND` | All log file writes | `Customer.h`, `Employee.h` |
| `fsync()` | Withdraw, transfer | `Customer.h` |
| `termios` / ECHO | Password input | `client.c` |
| `ftruncate()` | Loan counter | `Customer.h` |
| `htons()`/`htonl()`/`inet_addr()` | Server/client setup | `server.c`, `client.c` |
| `SO_REUSEADDR` | Server setup | `server.c` |
| `opendir()`/`readdir()` | Startup cleanup | `server.c` |
| `goto` | Password change flow | `Customer.h`, `Employee.h`, `Manager.h` |
| `getpid()` | File locking | All modules using fcntl |
| `bzero()` | Every I/O buffer use | All modules |

---

## Architecture Summary — How Concepts Layer Together

```
┌─────────────────────────────────────────────────────────┐
│                    CLIENT (client.c)                     │
│  socket() → connect() → read()/write() on socket fd     │
│  termios/ECHO for password hiding                       │
└────────────────────┬────────────────────────────────────┘
                     │ TCP (127.0.0.1:8080)
┌────────────────────▼────────────────────────────────────┐
│                   SERVER (server.c)                      │
│  socket() → setsockopt() → bind() → listen() → accept() │
│  fork() per client → connectionHandler()                 │
│  setupSignalHandlers() → cleanupSemaphore() on crash     │
└──────┬──────────┬──────────┬──────────┬─────────────────┘
       │          │          │          │
┌──────▼──┐ ┌────▼────┐ ┌───▼────┐ ┌──▼──────┐
│Customer │ │Employee │ │Manager │ │  Admin  │
│  .h     │ │  .h     │ │  .h    │ │  .h     │
└──┬──────┘ └───┬─────┘ └───┬────┘ └──┬──────┘
   │            │            │          │
   ▼            ▼            ▼          ▼
   ┌─────── Synchronization Layer ──────────────┐
   │  Named Semaphores (session per user)        │
   │  fcntl file locks (record per operation)    │
   └─────────────────────────────────────────────┘
   ▼            ▼            ▼          ▼
   ┌─────── File I/O Layer ─────────────────────┐
   │  open/read/write/close/lseek/fsync          │
   │  O_APPEND for logs | O_RDWR for updates     │
   └─────────────────────────────────────────────┘
   ▼            ▼            ▼          ▼
   ┌─────── Data Files (Data/) ─────────────────┐
   │  customers.txt    (struct Customer)          │
   │  employees.txt    (struct Employee)          │
   │  loanDetails.txt  (struct LoanDetails)       │
   │  loanCounter.txt  (struct Counter)           │
   │  trans_hist.txt   (struct trans_histroy)     │
   │  feedback.txt     (struct FeedBack)          │
   └─────────────────────────────────────────────┘
```

---

## Most Critical Facts for Any Interview

| Question Type | Key Answer |
|--------------|------------|
| How do you handle multiple clients? | `fork()` — one child process per client |
| How do you prevent concurrent balance corruption? | `fcntl()` record-level write lock (`F_SETLKW`) |
| How do you prevent duplicate logins? | POSIX named semaphore per user (`sem_trywait`) |
| How is data stored? | Binary structs in flat files; `open/read/write` system calls |
| How do you access specific records fast? | `lseek()` to exact byte offset (fixed-size structs) |
| How are passwords stored? | `crypt()` SHA-512 hash with salt — never plaintext |
| How does password input stay hidden? | `termios` — disable ECHO, restore after input |
| What happens if a client crashes? | Signal handler calls `sem_post()` before `_exit()` |
| What happens if the server crashes? | `cleanupAllSemaphores()` clears `/dev/shm/` on restart |
| Why TCP not UDP? | Reliability — financial data cannot be lost or reordered |
