#!/bin/bash

echo "========================================="
echo "COMPONENT STATUS CHECK"
echo "========================================="
echo ""

# Test 1: Server
echo "1. SERVER"
echo "   Starting server..."
./server > /tmp/server_test.log 2>&1 &
SERVER_PID=$!
sleep 1

if ps -p $SERVER_PID > /dev/null 2>&1; then
  echo "   ✓ Server is running (PID: $SERVER_PID)"
  kill $SERVER_PID 2>/dev/null
  wait $SERVER_PID 2>/dev/null
  echo "   ✓ Server stopped cleanly"
else
  echo "   ✗ Server failed to start"
  cat /tmp/server_test.log
fi
echo ""

# Test 2: Client
echo "2. CLIENT"
rm -f /tmp/test_client.db
RESULT=$(./client /tmp/test_client.db "SELECT 'Working' as status;" 2>&1)
if echo "$RESULT" | grep -q "Working"; then
  echo "   ✓ Client executes SQL: $RESULT"
else
  echo "   ✗ Client failed: $RESULT"
fi
rm -f /tmp/test_client.db
echo ""

# Test 3: Extension
echo "3. EXTENSION"
rm -f /tmp/test_ext.db
RESULT=$(echo "SELECT cloudsync_version();" | ./client_interactive /tmp/test_ext.db 2>&1)
if echo "$RESULT" | grep -q "0.8"; then
  echo "   ✓ Extension loaded: $(echo "$RESULT" | grep cloudsync_version)"
else
  echo "   ✗ Extension failed: $RESULT"
fi
rm -f /tmp/test_ext.db
echo ""

echo "========================================="
echo "STATUS: All components checked"
echo "========================================="
