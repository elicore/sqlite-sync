#!/bin/bash

echo "========================================="
echo "COMPREHENSIVE SYNC TEST"
echo "========================================="

# Clean up
rm -f test1.db test2.db server.db /tmp/sync_test.log

# Start server
echo "Starting server..."
./server > /tmp/sync_test.log 2>&1 &
SERVER_PID=$!
sleep 1

if ! ps -p $SERVER_PID > /dev/null; then
  echo "✗ Server failed to start"
  cat /tmp/sync_test.log
  exit 1
fi

echo "✓ Server running (PID: $SERVER_PID)"
echo ""

# Test Client 1 - Create and sync data
echo "=== Client 1: Creating data ==="
./client_interactive test1.db > /tmp/client1.log 2>&1 <<EOF
CREATE TABLE items (id TEXT PRIMARY KEY NOT NULL, value TEXT);
SELECT cloudsync_init('items');
SELECT cloudsync_network_init('http://localhost:8080/server.db');
INSERT INTO items (id, value) VALUES ('item-1', 'First Item');
INSERT INTO items (id, value) VALUES ('item-2', 'Second Item');
SELECT * FROM items;
SELECT cloudsync_network_sync();
EOF

CLIENT1_EXIT=$?
echo "Client 1 exit code: $CLIENT1_EXIT"
cat /tmp/client1.log
echo ""

# Give server time to process
sleep 2

# Test Client 2 - Sync and receive data
echo "=== Client 2: Syncing data ==="
./client_interactive test2.db > /tmp/client2.log 2>&1 <<EOF
CREATE TABLE items (id TEXT PRIMARY KEY NOT NULL, value TEXT);
SELECT cloudsync_init('items');
SELECT cloudsync_network_init('http://localhost:8080/server.db');
SELECT cloudsync_network_sync();
SELECT * FROM items;
EOF

CLIENT2_EXIT=$?
echo "Client 2 exit code: $CLIENT2_EXIT"
cat /tmp/client2.log
echo ""

# Stop server
echo "=== Server Log ==="
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
cat /tmp/sync_test.log
echo ""

# Check results
echo "=== Results ==="
if [ $CLIENT1_EXIT -ne 0 ]; then
  echo "✗ Client 1 failed (exit $CLIENT1_EXIT)"
elif [ $CLIENT2_EXIT -ne 0 ]; then
  echo "✗ Client 2 failed (exit $CLIENT2_EXIT)"
elif grep -q "First Item" /tmp/client2.log && grep -q "Second Item" /tmp/client2.log; then
  echo "✓ SUCCESS: Data synced from Client 1 to Client 2!"
else
  echo "✗ PARTIAL: Clients ran but data didn't sync"
fi

# Cleanup
rm -f test1.db test2.db server.db
