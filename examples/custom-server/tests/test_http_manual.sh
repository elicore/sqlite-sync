#!/bin/bash

echo "Testing server HTTP handling..."
./server 2>&1 &
SERVER_PID=$!
sleep 1

# Test 1: Simple GET request
echo "Test 1: GET /upload"
(
printf "GET /v1/cloudsync/test.db/ABC123/upload HTTP/1.1\r\n"
printf "Host: localhost:8080\r\n"
printf "Connection: close\r\n"
printf "\r\n"
) | nc localhost 8080

echo ""
echo "---"

# Test 2: POST /check
echo "Test 2: POST /check"
(
printf "POST /v1/cloudsync/test.db/ABC123/0/0/check HTTP/1.1\r\n"
printf "Host: localhost:8080\r\n"
printf "Content-Length: 0\r\n"
printf "Connection: close\r\n"
printf "\r\n"
) | nc localhost 8080

kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
