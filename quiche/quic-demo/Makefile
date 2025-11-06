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

# Mobile libs paths
MOBILE_LIBS_ROOT = ../../mobile_libs
MOBILE_LIBS_DIR = $(MOBILE_LIBS_ROOT)/$(PLATFORM)/$(ARCH)
MOBILE_LIBS_INCLUDE = $(MOBILE_LIBS_ROOT)/$(PLATFORM)/include
MOBILE_LIB = $(MOBILE_LIBS_DIR)/libquiche_engine.a
BUILD_SCRIPT = ../../build_mobile_libs.sh

# Compiler flags
INCLUDES = -I./include -I$(MOBILE_LIBS_INCLUDE) -I/usr/local/include
LDFLAGS = -L./lib -L/usr/local/lib $(FRAMEWORK_FLAGS)
# Note: mobile_libs contains libquiche_engine.a + libev.a, but we still need libquiche.a from lib/
LIBS = $(MOBILE_LIB) -L./lib -lquiche -lpthread -ldl -lm

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

# Check and build mobile libs if needed
.PHONY: check-mobile-libs
check-mobile-libs:
	@if [ ! -f "$(MOBILE_LIB)" ]; then \
		echo "Mobile library not found: $(MOBILE_LIB)"; \
		echo "Building mobile_libs for $(PLATFORM):$(ARCH)..."; \
		cd ../.. && ./build_mobile_libs.sh $(PLATFORM):$(ARCH); \
	else \
		echo "Using existing mobile library: $(MOBILE_LIB)"; \
	fi

all: check-mobile-libs $(BUILD_DIR) $(CLIENT) $(SERVER)

client: check-mobile-libs $(BUILD_DIR) $(CLIENT)
server: check-mobile-libs $(BUILD_DIR) $(SERVER)

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
