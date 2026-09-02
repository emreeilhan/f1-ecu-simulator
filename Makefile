CC := clang
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude

BUILD_DIR := build
TARGET := $(BUILD_DIR)/ecu_sim
TEST_TARGET := $(BUILD_DIR)/test_ecu

SOURCES := src/main.c src/ecu.c
TEST_SOURCES := tests/test_ecu.c src/ecu.c

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(SOURCES) include/ecu.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

$(TEST_TARGET): $(TEST_SOURCES) include/ecu.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_SOURCES) -o $(TEST_TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf $(BUILD_DIR)