#!/usr/bin/env python3
"""
Load testing client for the C++ message server.
Simulates 10,000 messages sent from multiple concurrent clients.
"""

import socket
import threading
import time
import random
import sys
from queue import Queue
from datetime import datetime

# Configuration
SERVER_HOST = 'localhost'
SERVER_PORT = 9090
TOTAL_MESSAGES = 10000
NUM_CLIENTS = 100  # Number of concurrent client connections
MESSAGES_PER_CLIENT = TOTAL_MESSAGES // NUM_CLIENTS

# Statistics
stats_lock = threading.Lock()
total_sent = 0
total_received = 0
failed_connections = 0
failed_sends = 0
start_time = None

# Sample messages to send
SAMPLE_MESSAGES = [
    "Hello from client",
    "Testing message priority",
    "High load test in progress",
    "Concurrent connection test",
    "Message processing verification",
    "Queue stress test",
    "Thread safety validation",
    "Server capacity test",
    "Network throughput check",
    "End-to-end latency measurement"
]

def update_stats(sent=0, received=0, failed_conn=0, failed_send=0):
    """Thread-safe statistics update"""
    global total_sent, total_received, failed_connections, failed_sends
    with stats_lock:
        total_sent += sent
        total_received += received
        failed_connections += failed_conn
        failed_sends += failed_send

def print_progress():
    """Print current progress"""
    with stats_lock:
        elapsed = time.time() - start_time
        print(f"\rProgress: Sent={total_sent}/{TOTAL_MESSAGES} | "
              f"Received={total_received} | "
              f"Failed={failed_connections + failed_sends} | "
              f"Time={elapsed:.1f}s | "
              f"Rate={total_sent/elapsed:.1f} msg/s", 
              end='', flush=True)

def client_worker(client_id, messages_to_send, result_queue):
    """
    Worker function for each client thread.
    Connects to server, sends messages, and receives acknowledgments.
    """
    sent = 0
    received = 0
    failed_send = 0
    
    try:
        # Create socket connection
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)  # 10 second timeout
        
        try:
            sock.connect((SERVER_HOST, SERVER_PORT))
        except Exception as e:
            update_stats(failed_conn=1)
            result_queue.put((client_id, 0, 0, 1, 0))
            return
        
        # Send messages
        for i in range(messages_to_send):
            message = random.choice(SAMPLE_MESSAGES)
            message_with_id = f"[Client-{client_id}][Msg-{i}] {message}\n"
            
            try:
                sock.sendall(message_with_id.encode())
                sent += 1
                update_stats(sent=1)
                
                # Try to receive acknowledgment
                try:
                    sock.settimeout(0.5)  # Short timeout for ACK
                    response = sock.recv(1024)
                    if response:
                        received += 1
                        update_stats(received=1)
                except socket.timeout:
                    pass  # ACK timeout is acceptable under high load
                
                # Small delay to avoid overwhelming the server
                time.sleep(0.001)  # 1ms delay between messages
                
            except Exception as e:
                failed_send += 1
                update_stats(failed_send=1)
        
        sock.close()
        
    except Exception as e:
        print(f"\nClient {client_id} error: {e}")
    
    result_queue.put((client_id, sent, received, 0, failed_send))

def run_load_test():
    """Main load test coordinator"""
    global start_time
    
    print("=" * 70)
    print(f"Load Test Configuration:")
    print(f"  Server: {SERVER_HOST}:{SERVER_PORT}")
    print(f"  Total Messages: {TOTAL_MESSAGES}")
    print(f"  Concurrent Clients: {NUM_CLIENTS}")
    print(f"  Messages per Client: {MESSAGES_PER_CLIENT}")
    print("=" * 70)
    print()
    
    # Test server connectivity first
    print("Testing server connectivity...")
    try:
        test_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        test_sock.settimeout(5)
        test_sock.connect((SERVER_HOST, SERVER_PORT))
        test_sock.close()
        print("✓ Server is reachable")
    except Exception as e:
        print(f"✗ Cannot connect to server: {e}")
        print("Make sure the server is running on port 9090")
        return
    
    print("\nStarting load test...")
    start_time = time.time()
    
    # Create result queue for collecting client results
    result_queue = Queue()
    
    # Create and start client threads
    threads = []
    for i in range(NUM_CLIENTS):
        t = threading.Thread(
            target=client_worker, 
            args=(i, MESSAGES_PER_CLIENT, result_queue)
        )
        t.start()
        threads.append(t)
        time.sleep(0.01)  # Small delay to stagger client connections
    
    # Progress monitoring
    progress_thread = threading.Thread(target=monitor_progress, args=(threads,))
    progress_thread.daemon = True
    progress_thread.start()
    
    # Wait for all clients to complete
    for t in threads:
        t.join()
    
    end_time = time.time()
    duration = end_time - start_time
    
    # Print final statistics
    print("\n\n" + "=" * 70)
    print("Load Test Results:")
    print("=" * 70)
    with stats_lock:
        print(f"Total Duration: {duration:.2f} seconds")
        print(f"Messages Sent: {total_sent}/{TOTAL_MESSAGES}")
        print(f"Acknowledgments Received: {total_received}")
        print(f"Failed Connections: {failed_connections}")
        print(f"Failed Sends: {failed_sends}")
        print(f"Average Rate: {total_sent/duration:.2f} messages/second")
        print(f"Success Rate: {(total_sent/(TOTAL_MESSAGES))*100:.2f}%")
    print("=" * 70)

def monitor_progress(threads):
    """Monitor and display progress while test is running"""
    while any(t.is_alive() for t in threads):
        print_progress()
        time.sleep(0.5)

def run_sustained_load_test(duration_seconds=60):
    """
    Alternative test: sustained load for a specific duration
    """
    global start_time
    
    print("=" * 70)
    print(f"Sustained Load Test Configuration:")
    print(f"  Server: {SERVER_HOST}:{SERVER_PORT}")
    print(f"  Duration: {duration_seconds} seconds")
    print(f"  Concurrent Clients: {NUM_CLIENTS}")
    print("=" * 70)
    print("\nStarting sustained load test...")
    
    start_time = time.time()
    end_time = start_time + duration_seconds
    
    result_queue = Queue()
    threads = []
    
    # Create persistent client connections
    for i in range(NUM_CLIENTS):
        t = threading.Thread(
            target=sustained_client_worker, 
            args=(i, end_time, result_queue)
        )
        t.start()
        threads.append(t)
    
    # Monitor progress
    while time.time() < end_time:
        print_progress()
        time.sleep(0.5)
    
    # Wait for all threads to finish
    for t in threads:
        t.join()
    
    duration = time.time() - start_time
    
    # Print results
    print("\n\n" + "=" * 70)
    print("Sustained Load Test Results:")
    print("=" * 70)
    with stats_lock:
        print(f"Duration: {duration:.2f} seconds")
        print(f"Total Messages Sent: {total_sent}")
        print(f"Total Acknowledgments: {total_received}")
        print(f"Average Rate: {total_sent/duration:.2f} messages/second")
    print("=" * 70)

def sustained_client_worker(client_id, end_time, result_queue):
    """Worker for sustained load test"""
    sent = 0
    received = 0
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((SERVER_HOST, SERVER_PORT))
        
        while time.time() < end_time:
            message = random.choice(SAMPLE_MESSAGES)
            message_with_id = f"[Client-{client_id}] {message}\n"
            
            try:
                sock.sendall(message_with_id.encode())
                sent += 1
                update_stats(sent=1)
                
                time.sleep(0.01)  # 10ms delay = ~100 msg/s per client
                
            except Exception as e:
                update_stats(failed_send=1)
                break
        
        sock.close()
        
    except Exception as e:
        update_stats(failed_conn=1)

if __name__ == "__main__":
    print("\nServer Load Testing Tool")
    print("-" * 70)
    print("Options:")
    print("  1. Send 10,000 messages (burst test)")
    print("  2. Sustained load test (60 seconds)")
    print("  3. Custom configuration")
    
    choice = input("\nSelect option (1-3): ").strip()
    
    if choice == "1":
        run_load_test()
    elif choice == "2":
        run_sustained_load_test(60)
    elif choice == "3":
        try:
            TOTAL_MESSAGES = int(input("Total messages: "))
            NUM_CLIENTS = int(input("Number of concurrent clients: "))
            MESSAGES_PER_CLIENT = TOTAL_MESSAGES // NUM_CLIENTS
            run_load_test()
        except ValueError:
            print("Invalid input!")
    else:
        print("Running default burst test...")
        run_load_test()