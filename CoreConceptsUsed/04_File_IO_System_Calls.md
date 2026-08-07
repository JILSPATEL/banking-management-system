# ⭐⭐⭐ File I/O & System Calls — open, read, write, lseek, fcntl, fsync
> **Priority: HIGH** — File I/O is fundamental to this project's data persistence strategy

---

## 1. The Core Design Decision — Files Instead of a Database

### Why Binary Files?
This project stores all data in flat binary files (`customers.txt`, `employees.txt`, etc.) instead of using a database like SQLite or MySQL. Understanding this choice is key for interviews.

**Advantages of Binary Files:**
- Demonstrates OS-level concepts (file descriptors, system calls, locking)
- O(1) random access to any record using `lseek()` — no SQL parser overhead
- Simple struct serialization — write/read entire structs with one call
- No external dependencies

**Disadvantages vs. a Real DB:**
- No indexing — finding a record requires O(n) sequential scan
- No transactions — crash mid-write can corrupt data
- No query language
- File path is hardcoded — not configurable

---

## 2. File Descriptors

### Concept Overview
A **file descriptor (fd)** is a small non-negative integer returned by `open()`, `socket()`, `pipe()`, etc. It is an index into the **per-process file descriptor table**, which points to entries in the **kernel's file description table**.

### Standard File Descriptors
| FD | Symbol | Device |
|----|--------|--------|
| 0 | `STDIN_FILENO` | Standard input (keyboard) |
| 1 | `STDOUT_FILENO` | Standard output (terminal) |
| 2 | `STDERR_FILENO` | Standard error |
| 3+ | User-opened | Files, sockets, pipes... |

### File Descriptors in This Project
Every `open()` call returns a new fd. Common fd's in a single connection handler:
- `connectionFD` — the TCP socket to the connected client
- `file` — the customers.txt or employees.txt
- `fp` — the trans_hist.txt for logging
- `file1` — the loanDetails.txt

### fd Inheritance and Fork()
After `fork()`, the child inherits all the parent's open fds. But each module function opens and closes its own fds locally — they are not left open across function calls (mostly). This avoids fd accumulation.

### Project Mapping — open() Calls

| Data File | Open Flags Used | In Function |
|-----------|----------------|-------------|
| `customers.txt` | `O_RDONLY` | `customerBal()` — read-only balance check |
| `customers.txt` | `O_CREAT | O_RDWR, 0644` | `depositMoney()`, `withdrawMoney()`, etc. |
| `employees.txt` | `O_CREAT | O_RDWR, 0644` | `loginEmployee()`, `addEmployee()` |
| `loanDetails.txt` | `O_RDWR | O_APPEND | O_CREAT, 0644` | `applyLoan()` — append new loan |
| `loanDetails.txt` | `O_CREAT | O_RDWR, 0644` | `approveRejectLoan()` — random write |
| `trans_hist.txt` | `O_RDWR | O_APPEND | O_CREAT, 0644` | All transaction-logging functions |
| `feedback.txt` | `O_CREAT | O_RDWR | O_APPEND, 0644` | `addFeedback()` |
| `loanCounter.txt` | `O_RDWR | O_CREAT, 0644` | `applyLoan()` |

---

## 3. open() — Opening Files

### Concept Overview
`open()` is the system call to open or create a file. It returns a file descriptor.

### Signature
```c
int open(const char *pathname, int flags, mode_t mode);
// mode is only used when O_CREAT is in flags
```

### File Open Flags Used in This Project

| Flag | Meaning | Where Used |
|------|---------|-----------|
| `O_RDONLY` | Read-only | `customerBal()`, `viewAssignedLoan()`, `readFeedBack()` |
| `O_RDWR` | Read and write | Most update operations |
| `O_WRONLY` | Write-only | Not used in this project |
| `O_CREAT` | Create if doesn't exist | Most data file opens |
| `O_APPEND` | Always write at end | Log files (trans_hist.txt, feedback.txt, loanDetails.txt) |
| `O_TRUNC` | Not used | Would truncate file on open |

### File Permission Mode — `0644`
When `O_CREAT` is used, the third argument sets permissions:
```
0644 = rw-r--r--
       │ │ │
       │ │ └── Others: read (4)
       │ └──── Group: read (4)
       └────── Owner: read+write (6)
```
This is an **octal** number. The leading `0` tells C compiler it's octal.

### umask Interaction
The actual permissions = `mode & ~umask`. If umask is `022`:
```
0644 & ~022 = 0644 & 0755 = 0644 (no change in this case)
```

---

## 4. read() and write() — Low-Level I/O

### Concept Overview
`read()` and `write()` are POSIX system calls for unbuffered I/O. Unlike `fread()`/`fwrite()` (which use FILE* buffering), these operate directly on file descriptors.

### Signatures
```c
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
```

### Binary Struct Serialization — How Data is Stored

The project writes entire structs as raw bytes:

```c
// Writing a Customer struct to file (from addCustomer in Employee.h)
write(file, &customer, sizeof(customer));

// Reading a Customer struct from file
read(file, &customer, sizeof(customer));
```

This creates a **flat binary file** where each record is exactly `sizeof(struct Customer)` bytes. The Nth record is at byte offset `N * sizeof(struct Customer)`.

**Why this enables O(1) access via lseek():**
Since every record is the same size, to access record N:
```c
lseek(file, N * sizeof(struct Customer), SEEK_SET);
read(file, &customer, sizeof(customer));
```

### Return Values to Check

| Return Value | Meaning |
|-------------|---------|
| > 0 | Number of bytes actually read/written |
| 0 | `read()` reached end-of-file (or peer closed connection for sockets) |
| -1 | Error; check `errno` |

**Partial read/write:** `read()` and `write()` may return less than `count`. For sockets this is common. For regular files with sufficient data, it usually returns the full amount. The project does not handle partial reads — an improvement point.

---

## 5. lseek() — Random Access Within Files

### Concept Overview
`lseek()` changes the **file offset** — the position where the next `read()` or `write()` will start. This enables **random access** to any record in the file without reading sequentially.

### Signature
```c
off_t lseek(int fd, off_t offset, int whence);
```

### The Three whence Values

| whence | Meaning | Formula |
|--------|---------|---------|
| `SEEK_SET` | From beginning of file | new_pos = offset |
| `SEEK_CUR` | From current position | new_pos = current + offset |
| `SEEK_END` | From end of file | new_pos = file_size + offset |

### lseek() Return Value
Returns the **new absolute file offset** from the beginning of the file. This is used to capture the record offset before locking:

```c
// From Customer.h depositMoney():
while(read(file, &customer, sizeof(customer)) != 0) {
    if (customer.accountNumber == accountNumber) { break; }
}
int offset = lseek(file, -sizeof(struct Customer), SEEK_CUR);
// offset now = byte position of the found customer record
// Used in struct flock to lock exactly that record
```

### lseek() Patterns Used in This Project

| Pattern | Call | Purpose |
|---------|------|---------|
| Go to file start | `lseek(file, 0, SEEK_SET)` | Reset for full sequential scan |
| Go to file end | `lseek(file, 0, SEEK_END)` | Append new record |
| Step back one record | `lseek(file, -sizeof(struct X), SEEK_CUR)` | Overwrite last-read record in place |
| Get current position | `lseek(file, 0, SEEK_CUR)` | Capture offset for locking |

---

## 6. fsync() — Forcing Data to Disk

### Concept Overview
`write()` copies data into the **kernel's page cache** (a buffer in RAM). The kernel writes to disk asynchronously. `fsync()` forces the kernel to flush all buffered data for a fd to the physical disk.

### Why This Matters for Banking
If the system crashes after `write()` but before the OS flushes to disk, data is lost. `fsync()` ensures durability — once it returns, data is on disk.

### Project Mapping

| File | Function | Where Used | Purpose |
|------|----------|-----------|---------|
| `Customer.h` | `withdrawMoney()` | After writing balance + transaction | Ensures withdrawal is durable |
| `Customer.h` | `transferFunds()` | After updating both accounts + history | Ensures transfer atomicity |

```c
// Customer.h withdrawMoney() — after write():
write(fp, &th, sizeof(th));
fsync(fp);   // Transaction history safely on disk
write(file, &customer, sizeof(customer));
fsync(file); // Updated balance safely on disk
```

**Interview point:** Not all writes use `fsync()` — `depositMoney()` does not. This is an inconsistency. In production, every financial write should be followed by fsync.

---

## 7. ftruncate() — Truncating a File

### Concept Overview
`ftruncate()` sets the file size to exactly `length` bytes. If the file was longer, the extra data is discarded. If shorter, it extends with null bytes.

### Project Mapping

| File | Function | Call | Purpose |
|------|----------|------|---------|
| `Customer.h` | `applyLoan()` | `ftruncate(file, sizeof(ct))` | Ensure loan counter file is exactly one struct in size |

The loan counter file (`loanCounter.txt`) stores exactly one `struct Counter` with a count field. `ftruncate()` prevents the file from growing with stale data.

---

## 8. O_APPEND — Safe Concurrent Appending

### Concept Overview
When `O_APPEND` is set, every `write()` atomically seeks to the end-of-file and writes. This is **atomic** at the OS level — even with multiple processes writing simultaneously, records don't interleave.

### Project Mapping

| File | Where | Purpose |
|------|-------|---------|
| `trans_hist.txt` | `depositMoney()`, `withdrawMoney()`, `transferFunds()`, etc. | Multiple processes can log transactions simultaneously |
| `loanDetails.txt` | `applyLoan()` | Multiple clients can apply for loans concurrently |
| `feedback.txt` | `addFeedback()` | Multiple clients can submit feedback simultaneously |

**Why O_APPEND is used:** Multiple child processes run `depositMoney()` simultaneously. Without O_APPEND, two processes could both read the end offset, then both write to the same position — one overwrites the other. O_APPEND makes the seek+write atomic.

---

## 9. Binary vs Text File I/O (fread/fwrite vs read/write)

This project uses **low-level POSIX I/O** (`open/read/write`) rather than **C standard I/O** (`fopen/fread/fwrite`).

| Aspect | POSIX (`open/read/write`) | C Standard (`fopen/fread/fwrite`) |
|--------|--------------------------|----------------------------------|
| Buffering | No user-space buffer | Buffered (stdio) |
| Locking | `fcntl()` (record-level) | `flockfile()` (whole file) |
| File descriptor | `int` (fd) | `FILE*` |
| System call count | Direct | Minimized by buffering |
| Portability | POSIX/Linux | C standard |
| Use in project | All data operations | `debug.c` uses `fopen/fread` |

**`debug.c` uses FILE* (fopen/fread/fwrite/rewind)** for reading and printing data — appropriate for a diagnostic/debugging tool that doesn't need locking.

---

## 10. Common Interview Questions

**Q1: What is the difference between read()/write() and fread()/fwrite()?**
> read()/write() are POSIX system calls that directly interact with the kernel with no user-space buffering. fread()/fwrite() are C standard library functions that add a user-space buffer — they reduce system calls by batching small reads/writes. This project uses read()/write() for system-call level control and because file locking with fcntl() integrates naturally with file descriptors.

**Q2: Can two processes write to the same file simultaneously without corruption?**
> With O_APPEND, two processes can append records atomically — the kernel ensures each write goes to the end without interleaving. For random writes (in-place updates), fcntl advisory locks must be used to coordinate. This project uses both: O_APPEND for logs, fcntl for in-place balance updates.

**Q3: What is the difference between lseek and fseek?**
> lseek() operates on file descriptors (POSIX). fseek() operates on FILE* pointers (C standard library). In this project, lseek() is used throughout for precise byte-level positioning needed for record-level locking.

**Q4: What is the kernel page cache?**
> The kernel maintains a cache of file data in RAM. read() copies from page cache to user buffer; write() copies from user buffer to page cache. The cache is written to disk asynchronously. fsync() forces immediate flush to disk. This cache dramatically improves I/O performance at the cost of data durability on crash.

**Q5: What happens if the server crashes mid-write?**
> Without fsync(), a write() that lands in the page cache may not reach disk before a crash. The struct on disk would be partially updated — corrupting the record. With fsync() (used in withdrawMoney and transferFunds), the kernel guarantees data is on disk before returning. This project only uses fsync() in some places — a production system should use it for every financial write.

**Q6: What is O(1) random access and how does lseek() enable it?**
> Since all records in a binary file have the same size (sizeof(struct Customer)), the byte offset of the Nth record is predictable: N * sizeof(struct Customer). lseek() jumps directly to that offset in O(1) time, without reading all preceding records.

---

## 11. Follow-up Questions

> **Q: Why are the data files named `.txt` but they contain binary data?**
> The `.txt` extension is misleading — these files contain raw binary struct data, not human-readable text. Opening them in a text editor shows garbage. The `debug.c` utility reads and prints them properly using fread(). The extension choice was cosmetic; the actual format is determined by how you open and read the file.

> **Q: How would you add indexing to speed up customer lookups?**
> Maintain a separate index file mapping account numbers to byte offsets in customers.txt. On each read, do a binary search in the index file to find the offset, then use lseek() to jump directly. Alternatively, use a hash map in memory at server startup.

> **Q: What is the difference between O_SYNC and fsync()?**
> O_SYNC is a flag on open() that makes every write() synchronous — it automatically calls fsync() after each write. This is simpler but adds latency to every write. fsync() is called explicitly when needed. This project uses the explicit fsync() approach.

---

## 12. Common Mistakes in Interviews

- Confusing file descriptor (int) with FILE* — they are different abstractions
- Saying `write()` guarantees data is on disk — it only writes to page cache unless fsync() is called
- Forgetting that lseek() returns the new offset — useful for capturing record positions
- Confusing `O_APPEND` atomicity with `O_TRUNC` — O_APPEND adds to end; O_TRUNC clears the file
- Not checking if lseek() failed (returns -1) — a seek beyond file size on read can cause read() to return 0 (EOF)

---

## 📌 Revision Box — One-Liners

| Concept | One-Line Explanation |
|---------|---------------------|
| File descriptor | Small integer indexing into the kernel's open file table |
| `open()` | System call to open/create a file; returns fd |
| `read()`/`write()` | Unbuffered POSIX I/O directly on fd; may return partial data |
| Binary struct I/O | Write/read entire structs as raw bytes for fixed-size records |
| `lseek(SEEK_CUR, -sizeof)` | Step back one record to overwrite in place |
| `lseek(SEEK_END, 0)` | Position at end of file for appending |
| `O_APPEND` | Atomic seek-to-end + write; safe for concurrent log appending |
| `O_CREAT` | Create file if it doesn't exist; requires mode (e.g., 0644) |
| `fsync()` | Flush kernel page cache to disk — ensures durability |
| `ftruncate()` | Set file to exact size; used to keep loan counter file clean |
| `0644` | File permissions: owner rw-, group r--, others r-- |

**Frequently Confused:**
- `read()`/`write()` (POSIX, fd-based) vs `fread()`/`fwrite()` (C stdlib, FILE*-based)
- `O_APPEND` (flag to open) vs `lseek(SEEK_END)` (manual positioning) — O_APPEND is atomic; manual lseek+write is not
- Page cache (RAM buffer) vs disk — write() fills cache; fsync() flushes to disk
