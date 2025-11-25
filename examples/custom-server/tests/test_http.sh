#!/bin/bash

# Clean up
rm -f client1.db client2.db server.db

# Start server in background
./server &
SERVER_PID=$!
sleep 1

echo "=== Setting up and syncing Client 1 ==="
./client_interactive client1.db <<EOF
CREATE TABLE items (id TEXT PRIMARY KEY NOT NULL, value TEXT);
SELECT cloudsync_init('items');
SELECT cloudsync_network_init('http://localhost:8080/server.db');
INSERT INTO items (id, value) VALUES ('test-id-1', 'item1');
SELECT cloudsync_network_sync();
SELECT * FROM items;
EOF

echo ""
echo "=== Setting up and syncing Client 2 ==="
./client_interactive client2.db <<EOF
CREATE TABLE items (id TEXT PRIMARY KEY NOT NULL, value TEXT);
SELECT cloudsync_init('items');
SELECT cloudsync_network_init('http://localhost:8080/server.db');
SELECT cloudsync_network_sync();
SELECT * FROM items;
EOF

# Stop server
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null

echo ""
echo "=== Test complete ==="
echo "If 'item1' appears in Client 2 output above, the sync worked!"
