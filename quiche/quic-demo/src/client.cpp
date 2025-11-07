// client.cpp
// QUIC Client Demo - Bidirectional Data Transfer Test with Polling
//
// Copyright (C) 2025, Cloudflare, Inc.
// All rights reserved.

#include <quiche_engine.h>

#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>

using namespace quiche;

// Global state
static std::atomic<bool> connection_ready(false);
static std::atomic<bool> should_stop(false);
static QuicheEngine* global_engine = nullptr;
static std::atomic<uint64_t> total_received(0);

// Data receiving thread - polls for data from server
static void dataReceivingThread() {
    // Wait for connection to be ready
    while (!connection_ready.load() && !should_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (should_stop.load()) {
        return;
    }

    std::cout << "✓ Starting data reception polling thread..." << std::endl;

    uint8_t buf[65536];  // 64KB buffer
    bool fin = false;

    // Poll for data until connection closes
    while (!should_stop.load()) {
        if (!global_engine) {
            break;
        }

        // Try to read data from stream
        ssize_t len = global_engine->read(buf, sizeof(buf), fin);

        if (len > 0) {
            // Data received
            total_received.fetch_add(len);
            std::cout << "✓ Received " << len << " bytes from server "
                     << "(total received: " << total_received.load() << " bytes)" << std::endl;
        } else if (len == 0) {
            // No data available, continue polling
        } else {
            // Error occurred
            std::cerr << "✗ Read error on stream 4" << std::endl;
            break;
        }

        if (fin) {
            std::cout << "✓ Server stream finished. Total received: "
                     << total_received.load() << " bytes" << std::endl;
            break;
        }

        // Small delay between polls to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Data sending thread - sends 200KB per second
static void dataSendingThread() {
    // Wait for connection to be ready
    while (!connection_ready.load() && !should_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (should_stop.load()) {
        return;
    }

    std::cout << "✓ Starting data transmission (200KB per second for 5 seconds)..." << std::endl;

    // Prepare 200KB data buffer
    const size_t CHUNK_SIZE = 200 * 1024;  // 200KB per second
    std::vector<uint8_t> data(CHUNK_SIZE);

    // Fill with pattern data
    for (size_t i = 0; i < CHUNK_SIZE; i++) {
        data[i] = static_cast<uint8_t>(i % 256);
    }

    uint64_t total_sent = 0;
    int count = 0;

    // Send data every second for 5 seconds
    while (!should_stop.load() && count < 5) {
        auto start = std::chrono::steady_clock::now();

        // Send 200KB on stream 4, in chunks
        if (global_engine) {
            size_t sent_this_round = 0;
            const size_t MAX_CHUNK = 65536;  // 64KB chunks

            for (size_t offset = 0; offset < data.size(); offset += MAX_CHUNK) {
                size_t chunk_size = std::min(MAX_CHUNK, data.size() - offset);

                ssize_t written = global_engine->write(data.data() + offset, chunk_size, false);

                if (written > 0) {
                    sent_this_round += written;
                    total_sent += written;
                } else if (written < 0) {
                    std::cerr << "✗ Failed to send chunk at offset " << offset << std::endl;
                    break;
                }

                // Small delay between chunks
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }

            count++;
            std::cout << "✓ Sent " << sent_this_round << " bytes in round " << count
                     << " (total sent: " << total_sent << " bytes)" << std::endl;
        }

        // Wait for 1 second
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto remaining = std::chrono::milliseconds(1000) - elapsed;

        if (remaining.count() > 0) {
            std::this_thread::sleep_for(remaining);
        }
    }

    std::cout << "✓ Data transmission completed. Total sent: " << total_sent << " bytes" << std::endl;

    // Close the stream with FIN
    if (global_engine) {
        global_engine->write(nullptr, 0, true);  // Send FIN
    }

    std::cout << "\n⏱ Waiting 8 seconds for server to complete sending remaining data..." << std::endl;

    // Wait longer for server to complete sending (8 seconds instead of 2)
    for (int i = 0; i < 8 && !should_stop.load(); i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "  " << (i + 1) << "/8 seconds..." << std::endl;
    }

    should_stop.store(true);
}

// Simplified event callback handler - no STREAM_READABLE event
static void onEngineEvent(
    QuicheEngine* engine,
    EngineEvent event,
    const EventData& event_data,
    void* user_data)
{
    (void)user_data;

    switch (event) {
        case EngineEvent::CONNECTED: {
            if (event_data.type == EventDataType::STRING) {
                std::cout << "✓ Connection established: " << event_data.str_val << std::endl;

                // Signal that connection is ready
                connection_ready.store(true);
            } else {
                std::cerr << "✗ Invalid event data for CONNECTED event" << std::endl;
            }
            break;
        }

        case EngineEvent::CONNECTION_CLOSED: {
            std::cout << "✓ Connection closed" << std::endl;

            // Print statistics
            if (engine) {
                EngineStats stats = engine->getStats();
                std::cout << "\n=== Connection Statistics ===" << std::endl;
                std::cout << "  Packets sent:     " << stats.packets_sent << std::endl;
                std::cout << "  Packets received: " << stats.packets_received << std::endl;
                std::cout << "  Bytes sent:       " << stats.bytes_sent << std::endl;
                std::cout << "  Bytes received:   " << stats.bytes_received << std::endl;
                std::cout << "  Packets lost:     " << stats.packets_lost << std::endl;
                std::cout << "  RTT:              " << stats.rtt_ns << " ns ("
                         << (stats.rtt_ns / 1000000.0) << " ms)" << std::endl;
                std::cout << "  CWND:             " << stats.cwnd << " bytes" << std::endl;
                std::cout << "\n=== Application Data ===" << std::endl;
                std::cout << "  Total received from server: " << total_received.load() << " bytes" << std::endl;
            }

            should_stop.store(true);
            break;
        }

        case EngineEvent::ERROR: {
            if (engine) {
                std::cerr << "✗ Engine error: " << engine->getLastError() << std::endl;
            }
            should_stop.store(true);
            break;
        }

        default:
            // Ignore other events (STREAM_READABLE not used in polling mode)
            break;
    }
}

int main(int argc, char* argv[]) {
    // Check arguments
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
        std::cerr << "\nExample:" << std::endl;
        std::cerr << "  " << argv[0] << " 127.0.0.1 4433" << std::endl;
        return 1;
    }

    const std::string host = argv[1];
    const std::string port = argv[2];

    std::cout << "QUIC Client Demo - Bidirectional Data Transfer (Polling Mode)" << std::endl;
    std::cout << "=============================================================" << std::endl;
    std::cout << "Upload:   200KB/sec for 5 seconds" << std::endl;
    std::cout << "Download: Polling for data from server" << std::endl;
    std::cout << "-------------------------------------------------------------" << std::endl;
    std::cout << "Connecting to " << host << ":" << port << "..." << std::endl << std::endl;

    // Prepare configuration using map with enum keys
    ConfigMap config;

    // QUIC configuration parameters - increased for bidirectional transfer
    config[ConfigKey::MAX_IDLE_TIMEOUT] = static_cast<uint64_t>(30000);          // 30 seconds
    config[ConfigKey::MAX_UDP_PAYLOAD_SIZE] = static_cast<uint64_t>(1350);       // 1350 bytes
    config[ConfigKey::INITIAL_MAX_DATA] = static_cast<uint64_t>(100000000);      // 100MB
    config[ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL] = static_cast<uint64_t>(50000000);   // 50MB
    config[ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE] = static_cast<uint64_t>(50000000);  // 50MB
    config[ConfigKey::INITIAL_MAX_STREAM_DATA_UNI] = static_cast<uint64_t>(10000000);          // 10MB
    config[ConfigKey::INITIAL_MAX_STREAMS_BIDI] = static_cast<uint64_t>(100);    // 100 streams
    config[ConfigKey::INITIAL_MAX_STREAMS_UNI] = static_cast<uint64_t>(100);     // 100 streams
    config[ConfigKey::DISABLE_ACTIVE_MIGRATION] = true;                          // Disable migration
    config[ConfigKey::ENABLE_DEBUG_LOG] = false;                                 // Debug logging off

    try {
        // Initialize engine with configuration map
        QuicheEngine engine(host, port, config);
        global_engine = &engine;

        // Set event callback
        if (!engine.setEventCallback(onEngineEvent, nullptr)) {
            std::cerr << "✗ Failed to set event callback" << std::endl;
            return 1;
        }

        // Start the engine (non-blocking, runs in background thread)
        std::cout << "Starting event loop..." << std::endl << std::endl;
        if (!engine.start()) {
            std::cerr << "\n✗ Engine error: " << engine.getLastError() << std::endl;
            return 1;
        }

        // Start data receiving thread (polling mode)
        std::thread receiver_thread(dataReceivingThread);

        // Start data sending thread
        std::thread sender_thread(dataSendingThread);

        // Wait for completion or timeout
        auto start_time = std::chrono::steady_clock::now();
        while (!should_stop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > 15) {
                std::cout << "\n⚠ Timeout reached, closing connection..." << std::endl;
                should_stop.store(true);
                break;
            }
        }

        // Wait for threads to finish
        if (receiver_thread.joinable()) {
            receiver_thread.join();
        }
        if (sender_thread.joinable()) {
            sender_thread.join();
        }

        // Print final statistics
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "Final Statistics" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "Total received from server: " << total_received.load() << " bytes" << std::endl;

        // Get engine statistics
        EngineStats stats = engine.getStats();
        std::cout << "\nConnection Statistics:" << std::endl;
        std::cout << "  Packets sent:     " << stats.packets_sent << std::endl;
        std::cout << "  Packets received: " << stats.packets_received << std::endl;
        std::cout << "  Packets lost:     " << stats.packets_lost << std::endl;
        std::cout << "  Bytes sent:       " << stats.bytes_sent << std::endl;
        std::cout << "  Bytes received:   " << stats.bytes_received << std::endl;
        std::cout << "  RTT:              " << (stats.rtt_ns / 1000000.0) << " ms" << std::endl;
        std::cout << "  CWND:             " << stats.cwnd << " bytes" << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        // Shutdown the engine (blocking, waits for graceful shutdown)
        engine.shutdown(0, "Test completed");

        // Clean up (automatic with destructor)
        std::cout << "\nCleaning up..." << std::endl;

        global_engine = nullptr;

    } catch (const std::exception& e) {
        std::cerr << "✗ Exception: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "✓ Done" << std::endl;
    return 0;
}
