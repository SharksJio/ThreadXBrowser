# ThreadXBrowser Makefile
# Build system for ThreadX WebSocket Browser Client

# Compiler settings
CC := gcc
ARM_CC := arm-none-eabi-gcc

# Build directories
BUILD_DIR := build
SRC_DIR := src

# Source files
SRCS := $(wildcard $(SRC_DIR)/*.c)
# Exclude threadx_netx_init.c and websocket_client.c from host simulation
HOST_SRCS := $(filter-out $(SRC_DIR)/threadx_netx_init.c $(SRC_DIR)/websocket_client.c, $(SRCS))
# Ensure stream_handler.c and ws_client_sim.c are included
HOST_SRCS := $(filter $(SRC_DIR)/main.c $(SRC_DIR)/stream_handler.c $(SRC_DIR)/ws_client_sim.c, $(HOST_SRCS))
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(HOST_SRCS))
ARM_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/arm/%.o,$(SRCS))

# Output binaries
HOST_TARGET := $(BUILD_DIR)/threadx_browser_sim
ARM_TARGET := $(BUILD_DIR)/threadx_browser_asr3605.elf

# Common flags
COMMON_CFLAGS := -Wall -Wextra -I$(SRC_DIR)

# Host simulation flags
HOST_CFLAGS := $(COMMON_CFLAGS) -O2 -g -DHOST_SIMULATION -pthread
HOST_LDFLAGS := -lpthread

# ARM cross-compile flags (ASR 3605)
ARM_CFLAGS := $(COMMON_CFLAGS) -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
ARM_CFLAGS += -DASR3605_TARGET -Os -ffunction-sections -fdata-sections
ARM_LDFLAGS := -Wl,--gc-sections -specs=nosys.specs

# ThreadX/NetX include paths (adjust for your installation)
THREADX_PATH ?= /opt/threadx
NETX_PATH ?= /opt/netx

ARM_CFLAGS += -I$(THREADX_PATH)/common/inc -I$(NETX_PATH)/common/inc

# Default target
.PHONY: all
all: host-sim

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/arm:
	mkdir -p $(BUILD_DIR)/arm

# Host simulation build
.PHONY: host-sim
host-sim: $(BUILD_DIR) $(HOST_TARGET)
	@echo "Host simulation build complete: $(HOST_TARGET)"

$(HOST_TARGET): $(OBJS)
	$(CC) $(HOST_CFLAGS) -o $@ $^ $(HOST_LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(HOST_CFLAGS) -c -o $@ $<

# ARM cross-compile build (ASR 3605)
.PHONY: asr3605
asr3605: $(BUILD_DIR)/arm $(ARM_TARGET)
	@echo "ASR 3605 build complete: $(ARM_TARGET)"

$(ARM_TARGET): $(ARM_OBJS)
	$(ARM_CC) $(ARM_CFLAGS) -o $@ $^ $(ARM_LDFLAGS)

$(BUILD_DIR)/arm/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)/arm
	$(ARM_CC) $(ARM_CFLAGS) -c -o $@ $<

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	@echo "Build directory cleaned"

# Run host simulation
.PHONY: run
run: host-sim
	./$(HOST_TARGET)

# Gateway server
.PHONY: gateway
gateway:
	cd gateway && npm install && npm start

# Gateway development mode (with auto-reload)
.PHONY: gateway-dev
gateway-dev:
	cd gateway && npm install && npm run dev

# Docker build and run
.PHONY: docker
docker:
	docker-compose up --build

# Docker build only
.PHONY: docker-build
docker-build:
	docker-compose build

# Docker cleanup
.PHONY: docker-clean
docker-clean:
	docker-compose down -v --rmi all

# Run tests
.PHONY: test
test:
	@echo "Running tests..."
	@if [ -f tests/run_tests.sh ]; then \
		./tests/run_tests.sh; \
	else \
		echo "No test runner found. Add tests to tests/ directory."; \
	fi

# Format code (requires clang-format)
.PHONY: format
format:
	@if command -v clang-format > /dev/null; then \
		find $(SRC_DIR) -name '*.c' -o -name '*.h' | xargs clang-format -i; \
		echo "Code formatted"; \
	else \
		echo "clang-format not found"; \
	fi

# Static analysis (requires cppcheck)
.PHONY: analyze
analyze:
	@if command -v cppcheck > /dev/null; then \
		cppcheck --enable=all --suppress=missingIncludeSystem $(SRC_DIR); \
	else \
		echo "cppcheck not found"; \
	fi

# Generate documentation (requires doxygen)
.PHONY: docs
docs:
	@if command -v doxygen > /dev/null; then \
		doxygen Doxyfile 2>/dev/null || echo "Doxyfile not found"; \
	else \
		echo "doxygen not found"; \
	fi

# Install dependencies
.PHONY: deps
deps:
	@echo "Installing gateway dependencies..."
	cd gateway && npm install
	@echo "Dependencies installed"

# Check toolchain
.PHONY: check-tools
check-tools:
	@echo "Checking build tools..."
	@echo -n "GCC: " && $(CC) --version | head -1 || echo "NOT FOUND"
	@echo -n "ARM GCC: " && $(ARM_CC) --version | head -1 || echo "NOT FOUND"
	@echo -n "Node.js: " && node --version || echo "NOT FOUND"
	@echo -n "npm: " && npm --version || echo "NOT FOUND"
	@echo -n "Docker: " && docker --version || echo "NOT FOUND"
	@echo -n "Docker Compose: " && docker-compose --version || echo "NOT FOUND"

# Help
.PHONY: help
help:
	@echo "ThreadXBrowser Build System"
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all           Build host simulation (default)"
	@echo "  host-sim      Build host simulation binary"
	@echo "  asr3605       Cross-compile for ASR 3605"
	@echo "  clean         Clean build artifacts"
	@echo "  run           Build and run host simulation"
	@echo "  gateway       Start Node.js gateway server"
	@echo "  gateway-dev   Start gateway in development mode"
	@echo "  docker        Build and run with Docker Compose"
	@echo "  docker-build  Build Docker images"
	@echo "  docker-clean  Remove Docker containers and images"
	@echo "  test          Run tests"
	@echo "  format        Format source code (requires clang-format)"
	@echo "  analyze       Run static analysis (requires cppcheck)"
	@echo "  docs          Generate documentation (requires doxygen)"
	@echo "  deps          Install dependencies"
	@echo "  check-tools   Check available build tools"
	@echo "  help          Show this help message"
	@echo ""
	@echo "Environment Variables:"
	@echo "  THREADX_PATH  Path to ThreadX installation (default: /opt/threadx)"
	@echo "  NETX_PATH     Path to NetX installation (default: /opt/netx)"
