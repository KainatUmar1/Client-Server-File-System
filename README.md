# Client-Server File System

## Overview
This project is a **Client-Server File System** implemented in **C** using **OS and networking principles**. It supports multiple clients and allows them to:
- Upload files
- Download files
- View file properties
- Synchronize file access using concurrency mechanisms

The system is designed to work on **Ubuntu (<= 22.04) with GCC compiler**, but it can also be adapted for Windows using the Winsock API.

---

## Features
### Phase 1: Basic Client-Server Communication
- Implemented a client and server in **C**.
- The client sends requests to the server for file operations.
- Server processes client requests and responds accordingly.
- **Supported Commands:**
  - **Upload (`$UPLOAD$<file_path>$`)**
    - Client requests to upload a file.
    - Server verifies available storage (each client gets **10 KB** storage).
    - If space is available, the file is uploaded and stored.
  - **View (`$VIEW$`)**
    - Client requests a list of stored files.
    - Server returns filenames, sizes, and timestamps.
  - **Download (`$DOWNLOAD$<file_name>$`)**
    - Client requests a specific file.
    - If the file exists, the server sends the data; otherwise, it responds with an error.

---

### Phase 2: Multithreading and Data Encoding
- **Multithreaded Server**
  - Server creates a new thread for each client connection.
  - The main thread only handles connections, while worker threads manage file operations.
- **Run-Length Encoding (RLE)**
  - Client encodes data before sending.
  - Server receives and decodes data.

---

### Phase 3: Synchronization with Mutex and Semaphore
- **Shared Memory Queue**
  - A **file handler function (X)** manages file read/write operations.
  - Client threads request file operations through **a queue**.
- **Synchronization Mechanisms**
  - **Mutex Locks** ensure only one client writes to the queue at a time.
  - **Semaphores** notify the file handler when data is available, preventing CPU overutilization.

---

### Phase 4: Custom Memory Allocator
- Implemented a custom memory allocator with:
  - **Dynamic memory management using `malloc` and `free`**.
  - **Bins** for efficient reuse of freed memory chunks.
  - **Fragmentation reduction techniques**.
  - **Requesting new memory pages** from the heap when needed.

---

## Installation & Usage
### Requirements
- **Ubuntu (<= 22.04)** or **Windows (Winsock API)**
- **GCC Compiler**

### Compilation
```sh
# Compile the server
gcc server.c -o server -lpthread

# Compile the client
gcc client.c -o client
```

### Running the Program
1. **Start the Server:**
   ```sh
   ./server
   ```
2. **Run a Client Instance:**
   ```sh
   ./client
   ```

### Example Usage
#### Upload a File
```sh
$UPLOAD$/home/user/sample.txt$
```
#### View Files
```sh
$VIEW$
```
#### Download a File
```sh
$DOWNLOAD$sample.txt$
```

---

## Future Improvements
- Add **encryption** for secure file transfers.
- Implement **graphical user interface (GUI)**.
- Extend support for **more file operations**.
- Optimize **memory management** further.

---

## Author
- **Kainat Umar** - *Developer of this `Client-Server File System` in `C` using OS principles.*
