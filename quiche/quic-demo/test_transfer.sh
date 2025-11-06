#!/bin/bash

# Kill any existing processes
pkill -f quic-server 2>/dev/null
pkill -f quic-client 2>/dev/null
sleep 1

echo "Starting QUIC server..."
./quic-server 127.0.0.1 4433 2>&1 | tee /tmp/server_output.log &
SERVER_PID=$!
sleep 2

echo "Starting QUIC client..."
./quic-client-cpp 127.0.0.1 4433

# Wait a bit for completion
sleep 2

# Kill server
kill $SERVER_PID 2>/dev/null

echo ""
echo "==================================="
echo "Server Output (Data Reception):"
echo "==================================="
grep -E "✓ Received|Stream.*finished|Total" /tmp/server_output.log | head -30

echo ""
echo "Test completed!"
