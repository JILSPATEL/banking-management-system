```markdown
# Final-BMS (Bank Management System)

A C-based client–server Bank Management System using TCP sockets, file-backed storage, advisory file locks, and POSIX semaphores. Supports Customer, Employee, Manager, and Admin workflows.

## Features
- Customer: deposit, withdraw, view balance, transfer, apply loan, change password, view transactions, feedback
- Employee: add customer, modify customer, approve/reject loans, view assigned loans, change password
- Manager: activate/deactivate accounts, assign loans, review feedback, change password
- Admin: add employee, modify customer/employee, manage roles
- Concurrency: record-level file locks; single active session per ID using POSIX named semaphores
- Wrong password: prints "Invalid Credential" and returns to main menu

## Requirements
- Linux environment
- gcc, libcrypt, pthreads
- Port 8080 available

## Build
```bash
# from project root
gcc server.c -o server -lcrypt -pthread
gcc client.c -o client
# or
make
```

## Run
```bash
# Terminal 1
./server

# Terminal 2
./client
```

## Default Data Files
- Data/employees.txt
- Data/customers.txt
- Data/loanDetails.txt
- Data/loanCounter.txt
- Data/trans_hist.txt
- Data/feedback.txt

## Login
- Admin: username/password = admin/admin (see `Modules/Admin.h`)
- Employee/Manager: stored in `Data/employees.txt` (`role`: 0=Manager, 1=Employee)
- Customer: stored in `Data/customers.txt` (`activeStatus`: 0/1)

## Notes
- Semaphores live in `/dev/shm` as `/sem_<ID>`; server cleans stale semaphores on start
- If `-lcrypt` link fails, install libxcrypt (distro-specific package)
- Change port by editing `server.c` and `client.c` consistently

## Project Structure
```
Final-BMS/
├─ AllStructure/allStruct.h       # Core structs: Customer, Employee, LoanDetails, etc.
├─ Modules/                       # Menus & actions for each role
│  ├─ Admin.h
│  ├─ Customer.h
│  ├─ Employee.h
│  └─ Manager.h
├─ Data/                          # File-backed storage
├─ server.c                       # TCP server, routing, session mgmt
├─ client.c                       # CLI client
├─ Makefile (optional)
└─ run.sh (optional)
```

## License
MIT (or update as needed)
```
