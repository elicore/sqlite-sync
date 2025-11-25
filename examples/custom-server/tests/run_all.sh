#!/bin/bash

# Consolidated test runner for custom sync server examples

cd "$(dirname "$0")/.."

echo "========================================="
echo "CUSTOM SYNC SERVER - TEST SUITE"
echo "========================================="
echo ""

# Build first
echo "Building..."
./build_server.sh > /dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "✗ Build failed"
  exit 1
fi
echo "✓ Build successful"
echo ""

# Test 1: Component Status
echo "=== Test 1: Component Status ==="
./tests/status_check.sh 2>&1 | grep -E "✓|✗"
echo ""

# Test 2: Comprehensive Sync Test
echo "=== Test 2: Comprehensive Sync Test ==="
./tests/test_comprehensive.sh 2>&1 | tail -5
echo ""

# Test 3: Final Integration Test
echo "=== Test 3: Final Integration Test ==="
./tests/test_final.sh 2>&1 | tail -5
echo ""

echo "========================================="
echo "TEST SUITE COMPLETE"
echo "========================================="
