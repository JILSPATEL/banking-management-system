# 🏦 Banking Management System — OS/Linux Interview Prep Index

> **Purpose:** This folder is your complete interview preparation guide for the Banking Management System project written in C on Linux. Every concept is mapped directly to your source code.

---

## 📁 Folder Structure

| File | Topics Covered | Priority |
|------|---------------|----------|
| `01_Networking_Sockets.md` | TCP/IP, socket(), bind(), listen(), accept(), connect(), Client-Server Architecture | ⭐⭐⭐ High |
| `02_Process_Management.md` | fork(), Parent-Child Process, Multi-Client Handling, File Descriptor Inheritance | ⭐⭐⭐ High |
| `03_Synchronization.md` | POSIX Named Semaphores, fcntl File Locking, Race Conditions, Critical Section | ⭐⭐⭐ High |
| `04_File_IO_System_Calls.md` | File Descriptors, open/read/write/close, lseek, fsync, Binary I/O, File Flags | ⭐⭐⭐ High |
| `05_Medium_Priority.md` | Signal Handling, Password Hashing (crypt), Terminal Control (termios), Error Handling | ⭐⭐ Medium |
| `06_Low_Priority.md` | Time API, Byte Order, SO_REUSEADDR, /dev/shm, Directory Traversal, Makefile | ⭐ Low |
| `07_Project_Mapping_Table.md` | Master feature-to-concept mapping table for every function in the project | 📋 Reference |

---

## 🏗️ Project Architecture (Quick View)

```
client.c  ──connect()──►  server.c (main)
                              │
                         accept() in loop
                              │
                         fork() per client
                              │
                    connectionHandler()
                    ┌──────────────────┐
                    │  customerMenu()  │──► Customer.h
                    │  employeeMenu()  │──► Employee.h
                    │  managerMenu()   │──► Manager.h
                    │  adminMenu()     │──► Admin.h
                    └──────────────────┘
                              │
                    Shared Binary Files (Data/)
                    ├── customers.txt
                    ├── employees.txt
                    ├── loanDetails.txt
                    ├── loanCounter.txt
                    ├── trans_hist.txt
                    └── feedback.txt
```

---

## ⚡ Quick Concept Lookup

| If asked about... | Go to file |
|------------------|-----------|
| How multiple clients are handled | `02_Process_Management.md` |
| How the server creates a socket | `01_Networking_Sockets.md` |
| How concurrent account access is prevented | `03_Synchronization.md` |
| How data is stored/read without a database | `04_File_IO_System_Calls.md` |
| How passwords are stored securely | `05_Medium_Priority.md` |
| How the password input is hidden | `05_Medium_Priority.md` |
| How signals clean up semaphores on crash | `05_Medium_Priority.md` |
| How duplicate logins are prevented | `03_Synchronization.md` |

---

## 🎯 Most Likely Interview Questions (Top 5)

1. **"How does your server handle multiple clients simultaneously?"** → `02_Process_Management.md`
2. **"What happens if two clients try to deposit to the same account at the same time?"** → `03_Synchronization.md`
3. **"Explain the TCP socket lifecycle in your project."** → `01_Networking_Sockets.md`
4. **"How do you store and retrieve data without a database?"** → `04_File_IO_System_Calls.md`
5. **"How does your login prevent the same user from logging in twice?"** → `03_Synchronization.md`
