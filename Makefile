CC := clang
CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -Wall -Wextra -Werror -Wpedantic
LDFLAGS :=
BUILD_DIR := build
CORE := src/ecu_core.c src/ecu_protocol.c src/ecu_time.c src/ecu_policy.c
HEADERS := $(wildcard include/*.h)
TESTS := $(BUILD_DIR)/test_ecu $(BUILD_DIR)/test_core $(BUILD_DIR)/test_transport $(BUILD_DIR)/test_scenarios
SAN_ASAN_OPTIONS := abort_on_error=1:detect_leaks=1
ifeq ($(shell uname -s),Darwin)
SAN_ASAN_OPTIONS := abort_on_error=1:detect_leaks=0
export SDKROOT ?= $(shell xcrun --show-sdk-path)
endif

.PHONY: all run test sanitize coverage coverage-gcc clean
all: $(BUILD_DIR)/ecu_sim
$(BUILD_DIR):
	mkdir -p $@
$(BUILD_DIR)/ecu_sim: src/main.c src/ecu_scenarios.c $(CORE) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c src/ecu_scenarios.c $(CORE) $(LDFLAGS) -o $@
$(BUILD_DIR)/test_ecu: tests/test_ecu.c src/ecu.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_ecu.c src/ecu.c $(LDFLAGS) -o $@
$(BUILD_DIR)/test_core: tests/test_core.c $(CORE) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_core.c $(CORE) $(LDFLAGS) -o $@
$(BUILD_DIR)/test_transport: tests/test_transport.c src/ecu_transport.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_transport.c src/ecu_transport.c $(LDFLAGS) -o $@
$(BUILD_DIR)/test_scenarios: tests/test_scenarios.c src/ecu_scenarios.c $(CORE) $(HEADERS) scenarios/expected.csv | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_scenarios.c src/ecu_scenarios.c $(CORE) $(LDFLAGS) -o $@
run: $(BUILD_DIR)/ecu_sim
	./$(BUILD_DIR)/ecu_sim
test: $(TESTS)
	./$(BUILD_DIR)/test_ecu
	./$(BUILD_DIR)/test_core
	./$(BUILD_DIR)/test_transport
	./$(BUILD_DIR)/test_scenarios
sanitize:
	ASAN_OPTIONS=$(SAN_ASAN_OPTIONS) UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 $(MAKE) BUILD_DIR=$(BUILD_DIR)/sanitize CFLAGS='-std=c11 -Wall -Wextra -Werror -Wpedantic -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test run
coverage:
	python3 tools/coverage.py --build-dir $(BUILD_DIR)/coverage --cc '$(CC)'
coverage-gcc:
	sh tools/coverage-gcc.sh
clean:
	rm -rf $(BUILD_DIR)
