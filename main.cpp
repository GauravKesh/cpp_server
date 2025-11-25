#include <bits/stdc++.h>
#include <thread>
#include <cstring>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <chrono>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

using namespace std;

/* Structure of message */
class Message
{
private:
    std::string clientId;
    long long timestamp;
    std::string text;
    int priority;

public:
    Message(std::string clientId, long long timestamp, std::string text, int priority)
        : clientId(std::move(clientId)), timestamp(timestamp),
          text(std::move(text)), priority(priority) {}

    const std::string &getClientId() const { return clientId; }
    const int getPriority() const { return priority; }
    const long long getTimestamp() const { return timestamp; }
    const std::string &getText() const { return text; }

    const std::string toString() const
    {
        std::stringstream ss;
        ss << "[" << clientId << "][" << timestamp << "][" << text << "][" << priority << "]";
        return ss.str();
    }
};

/* Message Comparator for handling process order */
class MessageComparator
{
public:
    bool operator()(const std::unique_ptr<Message> &a,
                    const std::unique_ptr<Message> &b) const
    {
        if (a->getPriority() != b->getPriority())
            return a->getPriority() < b->getPriority();
        return a->getTimestamp() > b->getTimestamp();
    }
};

class ThreadSafePriorityQueue
{
private:
    std::priority_queue<std::unique_ptr<Message>,
                        std::vector<std::unique_ptr<Message>>,
                        MessageComparator>
        queue;
    mutable std::mutex queueMutex;
    std::condition_variable queueConditionalVariable;
    std::atomic<bool> shutdown{false};

public:
    void push(std::unique_ptr<Message> &&msg) noexcept
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (shutdown)
                return; // Don't accept new messages during shutdown
            queue.push(std::move(msg));
        }
        queueConditionalVariable.notify_one();
    }

    std::unique_ptr<Message> pop()
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueConditionalVariable.wait(lock, [this]()
                                      { return !queue.empty() || shutdown; });

        if (shutdown && queue.empty())
        {
            return nullptr; // Signal shutdown
        }

        auto msg = std::move(const_cast<std::unique_ptr<Message> &>(queue.top()));
        queue.pop();
        return msg;
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        return queue.size();
    }

    void shutdownQueue()
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            shutdown = true;
        }
        queueConditionalVariable.notify_all();
    }
};

ThreadSafePriorityQueue messageQueue;

// Map individual clients to their sockets
std::unordered_map<std::string, int> clientSocketMap;
std::mutex socketMapMutex;

// Generate unique client IDs
std::atomic<int> clientCounter{0};

// Server running flag
std::atomic<bool> isServerRunning{true};

/* CONSUMER THREAD */
void consumerThread()
{
    std::cout << "Consumer thread started" << std::endl;

    while (isServerRunning)
    {
        auto msg = messageQueue.pop();

        if (!msg)
        {
            // nullptr means shutdown signal
            std::cout << "Consumer thread shutting down" << std::endl;
            break;
        }

        std::cout << msg->toString() << std::endl;

        std::string clientId = msg->getClientId();
        int clientSocket = -1;

        {
            std::lock_guard<std::mutex> lock(socketMapMutex);
            if (clientSocketMap.count(clientId))
            {
                clientSocket = clientSocketMap[clientId];
            }
        }

        if (clientSocket != -1)
        {
            std::string ack = "Received: " + msg->getText() + "\n";
            ssize_t sent = send(clientSocket, ack.c_str(), ack.size(), 0);
            if (sent < 0)
            {
                std::cerr << "Failed to send ACK to " << clientId << std::endl;
            }
        }
        else
        {
            std::cout << "Client socket not found for " << clientId << std::endl;
        }
    }

    std::cout << "Consumer thread exited" << std::endl;
}

void workerThread(int clientSocket, std::string clientId)
{
    std::cout << "Worker thread started for " << clientId << std::endl;
    char buffer[1024];

    while (isServerRunning)
    {
        memset(buffer, 0, sizeof(buffer));

        int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytes > 0)
        {
            std::cout << "Received " << bytes << " bytes from " << clientId << std::endl;

            std::string text(buffer, bytes);

            // Remove trailing newline if present
            if (!text.empty() && text.back() == '\n')
            {
                text.pop_back();
            }

            long long ts = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();

            auto msg = std::make_unique<Message>(clientId, ts, text, 1);
            messageQueue.push(std::move(msg));
        }
        else if (bytes == 0)
        {
            std::cout << "Client disconnected: " << clientId << std::endl;
            break;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }

            if (errno == EINTR)
            {
                continue; // Interrupted by signal, retry
            }

            std::cerr << "Read error from " << clientId << ": " << strerror(errno) << std::endl;
            break;
        }
    }

    // Cleanup
    {
        std::lock_guard<std::mutex> lock(socketMapMutex);
        clientSocketMap.erase(clientId);
    }
    close(clientSocket);
    std::cout << "Worker thread exited for " << clientId << std::endl;
}

// Signal handler for graceful shutdown
void signalHandler(int signum)
{
    std::cout << "\nReceived signal " << signum << ", shutting down..." << std::endl;
    isServerRunning = false;
    messageQueue.shutdownQueue();
}

/* Main function */
int main()
{
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return 1;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "setsockopt failed: " << strerror(errno) << std::endl;
        close(serverSocket);
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9090);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (::bind(serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, 10) < 0)
    {
        std::cerr << "Listen failed: " << strerror(errno) << std::endl;
        close(serverSocket);
        return 1;
    }

    std::thread consumer(consumerThread);

    std::cout << "Server listening on port 9090..." << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;

    while (isServerRunning)
    {
        // Set timeout for accept to allow checking isServerRunning
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int clientSocket = accept(serverSocket, nullptr, nullptr);

        if (clientSocket < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Timeout, continue loop to check isServerRunning
                continue;
            }
            if (!isServerRunning)
            {
                break; // Server is shutting down
            }
            std::cerr << "Error accepting connection: " << strerror(errno) << std::endl;
            continue;
        }

        std::string clientId = "client-" + std::to_string(clientCounter++);
        std::cout << "New connection: " << clientId << std::endl;

        {
            std::lock_guard<std::mutex> lock(socketMapMutex);
            clientSocketMap[clientId] = clientSocket;
        }

        std::thread(workerThread, clientSocket, clientId).detach();
    }

    // Cleanup
    std::cout << "Shutting down server..." << std::endl;

    // Close all client sockets
    {
        std::lock_guard<std::mutex> lock(socketMapMutex);
        for (auto &pair : clientSocketMap)
        {
            close(pair.second);
        }
        clientSocketMap.clear();
    }

    close(serverSocket);

    // Wait for consumer thread to finish
    if (consumer.joinable())
    {
        consumer.join();
    }

    std::cout << "Server shut down complete" << std::endl;
    return 0;
}