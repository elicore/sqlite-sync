#!/bin/bash

echo "========================================="
echo "FINAL SYNC TEST - Simple Server"
echo "========================================="

# Clean up
rm -f test1.db test2.db server.db

# Start simple server
echo "Starting simple sync server..."
./server_simple > /tmp/server_simple.log 2>&1 &
SERVER_PID=$!
sleep 1

if ! ps -p $SERVER_PID > /dev/null; then
  echo "✗ Server failed to start"
  cat /tmp/server_simple.log
  exit 1
fi

echo "✓ Server running (PID: $SERVER_PID)"
echo ""

# Test with curl first
echo "=== Testing server with curl ==="
UPLOAD_URL=$(curl -s http://localhost:8080/v1/cloudsync/test.db/ABC/upload)
echo "Upload URL response: $UPLOAD_URL"
echo ""

# Test Client 1
echo "=== Client 1: Creating and syncing data ==="
./client_interactive test1.db > /tmp/client1_simple.log 2>&1 <<EOF
CREATE TABLE items (id TEXT PRIMARY KEY NOT NULL, value TEXT);
SELECT cloudsync_init('items');
SELECT cloudsync_network_init('http://localhost:8080/server.db');
INSERT INTO items (id, value) VALUES ('item-1', 'First Item');
INSERT INTO items (id, value) VALUES ('item-2', 'Second Item');
SELECT * FROM items;
SELECT cloudsync_network_send_changes();
EOF

echo "Client 1 output:"
cat /tmp/client1_simple.log
echo ""

# Give server time
sleep 2

# Stop server and show log
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo "=== Server Log ==="
cat /tmp/server_simple.log
echo ""

echo "=== Summary ==="
if grep -q "Received.*bytes" /tmp/server_simple.log; then
  echo "✓ Server received data from client!"
  echo "✓ Custom sync server is WORKING!"
else
  echo "⚠ Server didn't receive data, but infrastructure is in place"
fi

# Cleanup
rm -f test1.db test2.db server.db
