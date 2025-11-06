# QUIC Demo - Client & Server
CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O2 -g
CXXFLAGS = -Wall -Wextra -O2 -g -std=c++11
INCLUDES = -I./include -I../engine/include -I/usr/local/include
LDFLAGS = -L./lib -L../engine/lib -L/usr/local/lib
LIBS = -lquiche_engine -lquiche -lev -lpthread -ldl -lm

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -framework Security -framework Foundation
endif

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

all: $(BUILD_DIR) $(CLIENT) $(SERVER)

client: $(BUILD_DIR) $(CLIENT)
server: $(BUILD_DIR) $(SERVER)

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

.PHONY: all client server clean
