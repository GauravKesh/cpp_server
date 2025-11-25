# Multi-threaded TCP Server in C++

A fully functional multi-threaded TCP server built from scratch using modern C++, without using any networking frameworks or external libraries.

This server:
- Accepts multiple clients concurrently
- Uses a dedicated worker thread per client
- Uses a consumer thread and a **thread-safe priority queue**
- Supports **acknowledgement responses** back to clients
- Demonstrates real-world concurrency, socket programming, and synchronization

---

## 🚀 Features

- `socket()`, `bind()`, `listen()`, `accept()`, `recv()`, `send()` implemented manually
- Worker-consumer architecture using:
  - `std::thread`
  - `std::mutex` & `std::lock_guard`
  - `std::condition_variable`
  - `std::priority_queue`
- Message object with timestamp & priority
- Safe shared resource handling using RAII and smart pointers

---

## 🛠 Build Instructions

### Compile
```bash
g++ main.cpp -o server -pthread
```

### Run Server
```bash
./server
```

### Connect Client (macOS / Linux)
```bash
nc localhost 9090
```

Then type messages (press Enter to send).

---

## 📂 Project Architecture

### Server Flow Diagram

```
                    ┌─────────────────────────────────────┐
                    │         SERVER START                │
                    │  1. Create socket                   │
                    │  2. Bind to port 9090              │
                    │  3. Listen for connections         │
                    │  4. Start consumer thread          │
                    └──────────────┬──────────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────────────┐
                    │      ACCEPT LOOP (Main Thread)       │
                    │  - Waits for client connections      │
                    │  - Creates unique client ID          │
                    │  - Stores socket in map              │
                    └──────────────┬───────────────────────┘
                                   │
                    ┌──────────────┴──────────────┐
                    │                             │
                    ▼                             ▼
        ┌───────────────────────┐     ┌───────────────────────┐
        │  WORKER THREAD        │     │  WORKER THREAD        │
        │  (Client 1)           │     │  (Client 2)           │
        │  ┌─────────────────┐  │     │  ┌─────────────────┐  │
        │  │ 1. recv() data  │  │     │  │ 1. recv() data  │  │
        │  │ 2. Create Message│ │     │  │ 2. Create Message│ │
        │  │    - clientId    │  │     │  │    - clientId    │  │
        │  │    - timestamp   │  │     │  │    - timestamp   │  │
        │  │    - text        │  │     │  │    - text        │  │
        │  │    - priority    │  │     │  │    - priority    │  │
        │  │ 3. Push to queue │  │     │  │ 3. Push to queue │  │
        │  └─────────────────┘  │     │  └─────────────────┘  │
        └───────────┬───────────┘     └───────────┬───────────┘
                    │                             │
                    └──────────────┬──────────────┘
                                   ▼
                    ┌──────────────────────────────────────┐
                    │   THREAD-SAFE PRIORITY QUEUE         │
                    │  ┌────────────────────────────────┐  │
                    │  │ Protected by mutex             │  │
                    │  │ Sorted by:                     │  │
                    │  │  1. Priority (high → low)      │  │
                    │  │  2. Timestamp (old → new)      │  │
                    │  │ Condition variable notifies    │  │
                    │  │ consumer when msg arrives      │  │
                    │  └────────────────────────────────┘  │
                    └──────────────┬───────────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────────────┐
                    │     CONSUMER THREAD                  │
                    │  ┌────────────────────────────────┐  │
                    │  │ 1. pop() from queue (blocking) │  │
                    │  │ 2. Log message to console      │  │
                    │  │ 3. Find client socket          │  │
                    │  │ 4. send() ACK to client        │  │
                    │  └────────────────────────────────┘  │
                    └──────────────┬───────────────────────┘
                                   │
                                   ▼
                            ┌─────────────┐
                            │   CLIENT    │
                            │ Receives ACK│
                            └─────────────┘
```

### Detailed Message Flow

```
┌─────────┐                                              ┌─────────┐
│ CLIENT  │                                              │ SERVER  │
└────┬────┘                                              └────┬────┘
     │                                                        │
     │  1. TCP Connection (nc localhost 9090)                │
     ├───────────────────────────────────────────────────────▶
     │                                                        │
     │                        2. accept() creates worker thread
     │                           clientId = "client-0"        │
     │                           Store in clientSocketMap    │
     │                                                        │
     │  3. Type message: "Hello Server"                      │
     ├───────────────────────────────────────────────────────▶
     │                                                        │
     │                   4. Worker Thread recv() ────┐       │
     │                      Creates Message object   │       │
     │                      - clientId: "client-0"   │       │
     │                      - timestamp: 1234567890  │       │
     │                      - text: "Hello Server"   │       │
     │                      - priority: 1            │       │
     │                                               │       │
     │                   5. queue.push(msg) ─────────┤       │
     │                      notify_one()             │       │
     │                                               │       │
     │                   6. Consumer Thread wakes up │       │
     │                      queue.pop()              │       │
     │                      Prints: [client-0][...]  │       │
     │                                               │       │
     │                   7. Lookup clientSocketMap   │       │
     │                      Find socket for client-0 │       │
     │                                               │       │
     │  8. ACK: "Received: Hello Server"                     │
     │◀───────────────────────────────────────────────────────
     │                      send() ──────────────────┘       │
     │                                                        │
     │  9. Client sees response                              │
     │                                                        │
```

### Component Breakdown

```
Main Thread
  └─ Accept Loop ──┬─▶ Worker Thread 1 ──┐
                   ├─▶ Worker Thread 2 ──┤
                   ├─▶ Worker Thread 3 ──┼─▶ Priority Queue ──▶ Consumer Thread
                   ├─▶ Worker Thread 4 ──┤
                   └─▶ Worker Thread N ──┘
```

---

## 🧠 What I Learned

This project was a complete learning experience from the ground up. I gained deep understanding of:
- TCP networking fundamentals and how servers work internally
- Threading concepts - how threads are created, synchronized, and managed
- The entire lifecycle of a server: socket creation, binding, listening, accepting connections
- Internal phases of request handling: receive → queue → process → acknowledge
- Synchronization, race conditions, and thread safety mechanisms
- Message prioritization concepts and queue management
- Real backend system design patterns and architecture

I built this entirely from scratch while learning with ChatGPT guidance. It helped me understand not just "how to code it" but "how it actually works" - from the low-level socket operations to high-level threading patterns. Every line of code taught me something new about systems programming and concurrent server architecture.

---

## ✨ Future Enhancements

- Thread pool instead of thread-per-client
- Priority parsing (`/urgent message`)
- Graceful shutdown
- Broadcast chat
- Logging to file

---

## 🤝 Acknowledgements

Special thanks to ChatGPT for guiding conceptual understanding and architectural direction while I implemented all logic manually.

---

## 📜 License

MIT License