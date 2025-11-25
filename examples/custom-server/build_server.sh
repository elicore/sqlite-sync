#!/bin/bash

echo "Building custom sync server examples..."

gcc -o build/server src/server.c ../../sqlite/sqlite3.c \
    -I../../src -I../../sqlite \
    -DCLOUDSYNC_OMIT_NETWORK \
    -DSQLITE_CORE \
    -framework Security \
    -g

gcc -o build/client src/client.c ../../sqlite/sqlite3.c \
    -I. -I../../sqlite \
    -DSQLITE_CORE \
    -g

gcc -o build/client_interactive src/client_interactive.c ../../sqlite/sqlite3.c \
    -I. -I../../sqlite \
    -DSQLITE_CORE \
    -g

gcc -o build/server_simple src/server_simple.c ../../sqlite/sqlite3.c \
    -I../../sqlite \
    -g

echo "Build complete! Binaries in build/ directory"
