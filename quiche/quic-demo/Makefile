# QUIC Demo - Client & Server
CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O2 -g
CXXFLAGS = -Wall -Wextra -O2 -g -std=c++11

# Detect platform and architecture
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Determine platform-specific settings
ifeq ($(UNAME_S),Darwin)
    PLATFORM = macos
    FRAMEWORK_FLAGS = -framework Security -framework Foundation
else ifeq ($(UNAME_S),Linux)
    PLATFORM = linux
    FRAMEWORK_FLAGS =
else
    $(error Unsupported platform: $(UNAME_S))
endif

# Determine architecture
ifeq ($(UNAME_M),x86_64)
    ARCH = x86_64
else ifeq ($(UNAME_M),arm64)
    ARCH = arm64
else ifeq ($(UNAME_M),aarch64)
    ARCH = arm64
else
    $(error Unsupported architecture: $(UNAME_M))
endif

# Library paths
LIB_ROOT = ../../lib
LIB_DIR = $(LIB_ROOT)/$(PLATFORM)/$(ARCH)
INCLUDE_ROOT = ../../include
QUICHE_ENGINE_LIB = $(LIB_DIR)/libquiche_engine.a
BUILD_SCRIPT = ../../quiche_engine_all.sh

# Compiler flags
INCLUDES = -I./include -I$(INCLUDE_ROOT) -I/usr/local/include
LDFLAGS = -L./lib -L/usr/local/lib $(FRAMEWORK_FLAGS)
# Note: libquiche_engine.a contains libquiche.a + libev.a + BoringSSL
LIBS = $(QUICHE_ENGINE_LIB) -L./lib -lquiche -lpthread -ldl -lm

SRC_DIR = src
BUILD_DIR = build
CLIENT = quic-client
SERVER = quic-server

# Source files
SERVER_SRC = $(SRC_DIR)/server.c
CLIENT_SRC = $(SRC_DIR)/client.cpp

# Object files
SERVER_OBJ = $(BUILD_DIR)/server.o
CLIENT_OBJ = $(BUILD_DIR)/client.o

# Check and build quiche_engine library if needed
.PHONY: check-quiche-engine
check-quiche-engine:
	@if [ ! -f "$(QUICHE_ENGINE_LIB)" ]; then \
		echo "quiche_engine library not found: $(QUICHE_ENGINE_LIB)"; \
		echo "Building quiche_engine for $(PLATFORM) $(ARCH)..."; \
		cd ../.. && $(BUILD_SCRIPT) $(PLATFORM) $(ARCH); \
	else \
		echo "Using existing quiche_engine library: $(QUICHE_ENGINE_LIB)"; \
	fi

all: check-quiche-engine $(BUILD_DIR) $(CLIENT) $(SERVER)

client: check-quiche-engine $(BUILD_DIR) $(CLIENT)
server: check-quiche-engine $(BUILD_DIR) $(SERVER)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(CLIENT): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "Built $@ successfully"

$(SERVER): $(SERVER_OBJ)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "Built $@ successfully"

$(CLIENT_OBJ): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(SERVER_OBJ): $(SERVER_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) $(CLIENT) $(SERVER)
	@echo "Cleaned"

.PHONY: all client server clean check-mobile-libs
