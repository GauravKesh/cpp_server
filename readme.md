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

```
Server
 ├── Accept loop  (creates thread per client)
 ├── Worker threads (recv messages → queue)
 └── Consumer thread (pop → process → send ack)
```

### Message Flow

```
Client → workerThread(recv) → PriorityQueue.push() → consumerThread.pop() → send ACK → Client
```

---

## 🧠 What I Learned

This project helped me deeply understand:
- TCP networking fundamentals
- Blocking vs non-blocking socket I/O
- Synchronization, race conditions, and thread safety
- Message prioritization concepts and queue management
- Real backend system design patterns

I built this step-by-step while learning using ChatGPT guidance. It helped clarify low-level networking, threading, and design concepts efficiently.

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