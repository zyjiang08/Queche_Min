# Makefile for QUIC Demo Client and Server
# This builds the example QUIC client and server using quiche library

# Compilers and flags
CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O2 -g
CXXFLAGS = -Wall -Wextra -O2 -g -std=c++17
INCLUDES = -I./include -I/usr/local/include
LDFLAGS = -L./lib -L/usr/local/lib
LIBS = -lquiche -lev -lpthread -ldl -lm

# Check for macOS vs Linux
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS specific flags
    LDFLAGS += -framework Security -framework Foundation
endif

# Source and output directories
SRC_DIR = src
BUILD_DIR = build

# Targets
CLIENT = quic-client
CLIENT_CPP = quic-client-cpp
SERVER = quic-server

# C Source files (original)
ENGINE_C_SRC = $(SRC_DIR)/quiche_engine.c
CLIENT_C_SRC = $(SRC_DIR)/client.c
SERVER_C_SRC = $(SRC_DIR)/server.c

# C++ Source files (modularized)
ENGINE_IMPL_CPP_SRC = $(SRC_DIR)/quiche_engine_impl.cpp
ENGINE_API_CPP_SRC = $(SRC_DIR)/quiche_engine_api.cpp
THREAD_UTILS_CPP_SRC = $(SRC_DIR)/thread_utils.cpp
CLIENT_CPP_SRC = $(SRC_DIR)/client.cpp

# C Object files
ENGINE_C_OBJ = $(BUILD_DIR)/quiche_engine_c.o
CLIENT_C_OBJ = $(BUILD_DIR)/client_c.o
SERVER_C_OBJ = $(BUILD_DIR)/server_c.o

# C++ Object files
ENGINE_IMPL_CPP_OBJ = $(BUILD_DIR)/quiche_engine_impl.o
ENGINE_API_CPP_OBJ = $(BUILD_DIR)/quiche_engine_api.o
THREAD_UTILS_CPP_OBJ = $(BUILD_DIR)/thread_utils.o
CLIENT_CPP_OBJ = $(BUILD_DIR)/client.o

# All C++ engine objects
ENGINE_CPP_OBJS = $(ENGINE_IMPL_CPP_OBJ) $(ENGINE_API_CPP_OBJ) $(THREAD_UTILS_CPP_OBJ)

# Default target - build C++ version by default
all: $(BUILD_DIR) $(CLIENT_CPP) $(SERVER)

# Build C version
c-client: $(BUILD_DIR) $(CLIENT)

# Build C++ version
cpp-client: $(BUILD_DIR) $(CLIENT_CPP)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build C++ client (modularized engine)
$(CLIENT_CPP): $(CLIENT_CPP_OBJ) $(ENGINE_CPP_OBJS)
	$(CXX) $(CXXFLAGS) $(CLIENT_CPP_OBJ) $(ENGINE_CPP_OBJS) $(LDFLAGS) $(LIBS) -o $(CLIENT_CPP)
	@echo "Built $(CLIENT_CPP) successfully"

# Build C client (original)
$(CLIENT): $(CLIENT_C_OBJ) $(ENGINE_C_OBJ)
	$(CC) $(CFLAGS) $(CLIENT_C_OBJ) $(ENGINE_C_OBJ) $(LDFLAGS) $(LIBS) -o $(CLIENT)
	@echo "Built $(CLIENT) successfully"

# Build server (C version)
$(SERVER): $(SERVER_C_OBJ)
	$(CC) $(CFLAGS) $(SERVER_C_OBJ) $(LDFLAGS) $(LIBS) -o $(SERVER)
	@echo "Built $(SERVER) successfully"

# Compile C++ engine implementation object
$(ENGINE_IMPL_CPP_OBJ): $(ENGINE_IMPL_CPP_SRC) $(SRC_DIR)/quiche_engine_impl.h $(SRC_DIR)/quiche_engine.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $(ENGINE_IMPL_CPP_SRC) -o $(ENGINE_IMPL_CPP_OBJ)

# Compile C++ engine API object
$(ENGINE_API_CPP_OBJ): $(ENGINE_API_CPP_SRC) $(SRC_DIR)/quiche_engine.h $(SRC_DIR)/quiche_engine_impl.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $(ENGINE_API_CPP_SRC) -o $(ENGINE_API_CPP_OBJ)

# Compile C++ thread utils object
$(THREAD_UTILS_CPP_OBJ): $(THREAD_UTILS_CPP_SRC) $(SRC_DIR)/thread_utils.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $(THREAD_UTILS_CPP_SRC) -o $(THREAD_UTILS_CPP_OBJ)

# Compile C++ client object
$(CLIENT_CPP_OBJ): $(CLIENT_CPP_SRC) $(SRC_DIR)/quiche_engine.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $(CLIENT_CPP_SRC) -o $(CLIENT_CPP_OBJ)

# Compile C engine object
$(ENGINE_C_OBJ): $(ENGINE_C_SRC) $(SRC_DIR)/quiche_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(ENGINE_C_SRC) -o $(ENGINE_C_OBJ)

# Compile C client object
$(CLIENT_C_OBJ): $(CLIENT_C_SRC) $(SRC_DIR)/quiche_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(CLIENT_C_SRC) -o $(CLIENT_C_OBJ)

# Compile C server object
$(SERVER_C_OBJ): $(SERVER_C_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(SERVER_C_SRC) -o $(SERVER_C_OBJ)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(CLIENT) $(CLIENT_CPP) $(SERVER) quic-client-old
	@echo "Cleaned build artifacts"

# Run C++ client (example: make run-cpp-client HOST=cloudflare-quic.com PORT=443)
run-cpp-client:
	./$(CLIENT_CPP) $(HOST) $(PORT)

# Run C client (example: make run-c-client HOST=cloudflare-quic.com PORT=443)
run-c-client:
	./$(CLIENT) $(HOST) $(PORT)

# Run server (example: make run-server HOST=127.0.0.1 PORT=4433)
run-server:
	./$(SERVER) $(HOST) $(PORT)

# Help target
help:
	@echo "QUIC Demo Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all             - Build C++ client and server (default)"
	@echo "  cpp-client      - Build C++ client (map-based config)"
	@echo "  c-client        - Build C client (thread-safe)"
	@echo "  server          - Build server"
	@echo "  clean           - Remove build artifacts"
	@echo "  run-cpp-client  - Run C++ client (set HOST and PORT variables)"
	@echo "  run-c-client    - Run C client (set HOST and PORT variables)"
	@echo "  run-server      - Run server (set HOST and PORT variables)"
	@echo "  help            - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make                                      # Build C++ version"
	@echo "  make cpp-client                           # Build C++ client"
	@echo "  make c-client                             # Build C client"
	@echo "  make run-cpp-client HOST=127.0.0.1 PORT=4433"
	@echo "  make run-c-client HOST=127.0.0.1 PORT=4433"
	@echo "  make run-server HOST=127.0.0.1 PORT=4433"

# Phony targets
.PHONY: all cpp-client c-client clean run-cpp-client run-c-client run-server help
