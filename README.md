# Threaded Key-Value Store

A simple multi-threaded, in-memory key-value store written in C. The system consists of a server that handles multiple client connections concurrently using POSIX threads, and a client program for interacting with the server.

## Features

- Multi-threaded server using pthreads
- Thread-safe hash table protected by mutex
- Simple text-based protocol
- TCP/IP networking on localhost

## Building

1. Create a build directory:
```bash
mkdir build
cd build
```

2. Run CMake:
```bash
cmake ..
```

3. Build the project:
```bash
make
```

This will create two executables: `server` and `client`.

**Note:** The `uthash.h` file must be in the same directory as `server.c` for the build to succeed. It's already included in this project.

## Running

1. Start the server in one terminal:
```bash
./server
```

The server will listen on port 8888.

2. In another terminal, run the client with commands:
```bash
./client SET name Hong
./client GET name
./client DELETE name
```

## Supported Commands

- **SET key value**: Store a key-value pair. Returns "OK" on success.
- **GET key**: Retrieve the value for a key. Returns the value or "NOT_FOUND" if the key doesn't exist.
- **DELETE key**: Remove a key-value pair. Returns "OK" on success.

## Requirements

- CMake 3.10 or higher
- C compiler with pthread support (gcc or clang)
- uthash.h library (included in the project directory)

## Project Structure

- `server.c`: Multi-threaded server implementation
- `client.c`: Client implementation
- `uthash.h`: Hash table library (required by server.c)
- `CMakeLists.txt`: Build configuration
- `README.md`: This file
