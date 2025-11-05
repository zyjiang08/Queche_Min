# Makefile for QUIC Demo Client and Server
# This builds the example QUIC client and server using quiche library

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
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
SERVER = quic-server

# Source files
CLIENT_SRC = $(SRC_DIR)/client.c
SERVER_SRC = $(SRC_DIR)/server.c

# Object files
CLIENT_OBJ = $(BUILD_DIR)/client.o
SERVER_OBJ = $(BUILD_DIR)/server.o

# Default target
all: $(BUILD_DIR) $(CLIENT) $(SERVER)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build client
$(CLIENT): $(CLIENT_OBJ)
	$(CC) $(CFLAGS) $(CLIENT_OBJ) $(LDFLAGS) $(LIBS) -o $(CLIENT)
	@echo "Built $(CLIENT) successfully"

# Build server
$(SERVER): $(SERVER_OBJ)
	$(CC) $(CFLAGS) $(SERVER_OBJ) $(LDFLAGS) $(LIBS) -o $(SERVER)
	@echo "Built $(SERVER) successfully"

# Compile client object
$(CLIENT_OBJ): $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(CLIENT_SRC) -o $(CLIENT_OBJ)

# Compile server object
$(SERVER_OBJ): $(SERVER_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(SERVER_SRC) -o $(SERVER_OBJ)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(CLIENT) $(SERVER)
	@echo "Cleaned build artifacts"

# Run client (example: make run-client HOST=cloudflare-quic.com PORT=443)
run-client:
	./$(CLIENT) $(HOST) $(PORT)

# Run server (example: make run-server HOST=127.0.0.1 PORT=4433)
run-server:
	./$(SERVER) $(HOST) $(PORT)

# Help target
help:
	@echo "QUIC Demo Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build both client and server (default)"
	@echo "  client       - Build only the client"
	@echo "  server       - Build only the server"
	@echo "  clean        - Remove build artifacts"
	@echo "  run-client   - Run the client (set HOST and PORT variables)"
	@echo "  run-server   - Run the server (set HOST and PORT variables)"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make"
	@echo "  make client"
	@echo "  make run-client HOST=cloudflare-quic.com PORT=443"
	@echo "  make run-server HOST=127.0.0.1 PORT=4433"

# Phony targets
.PHONY: all clean run-client run-server help
