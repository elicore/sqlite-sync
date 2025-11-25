# Custom Sync Server Example

SQLITE_DIR = ../../sqlite
SRC_DIR = src
BUILD_DIR = build
TESTS_DIR = tests

CC = gcc
CFLAGS = -Wall -Wextra -I$(SQLITE_DIR) -I../../src -g
LDFLAGS = -framework Security

SOURCES = $(wildcard $(SRC_DIR)/*.c)
TARGETS = $(BUILD_DIR)/server $(BUILD_DIR)/server_simple $(BUILD_DIR)/client $(BUILD_DIR)/client_interactive

.PHONY: all clean test

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR):
\tmkdir -p $(BUILD_DIR)

$(BUILD_DIR)/server: $(SRC_DIR)/server.c $(SQLITE_DIR)/sqlite3.c
\t$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/server_simple: $(SRC_DIR)/server_simple.c $(SQLITE_DIR)/sqlite3.c
\t$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/client: $(SRC_DIR)/client.c $(SQLITE_DIR)/sqlite3.c
\t$(CC) $(CFLAGS) -DSQLITE_CORE -o $@ $^

$(BUILD_DIR)/client_interactive: $(SRC_DIR)/client_interactive.c $(SQLITE_DIR)/sqlite3.c
\t$(CC) $(CFLAGS) -DSQLITE_CORE -o $@ $^

clean:
\trm -rf $(BUILD_DIR)/*
\trm -f *.db *.log

test: all
\t@echo "Running tests..."
\t@cd $(TESTS_DIR) && ./status_check.sh

.DEFAULT_GOAL := all
