# QUIC Demo - Client & Server
CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O2 -g
CXXFLAGS = -Wall -Wextra -O2 -g -std=c++11
INCLUDES = -I./include -I../../engine/include -I../../engine/src -I/usr/local/include
LDFLAGS = -L./lib -L/usr/local/lib
LIBS = -lquiche -lev -lpthread -ldl -lm

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
ENGINE_IMPL_SRC = ../../engine/src/quiche_engine_impl.cpp
ENGINE_API_SRC = ../../engine/src/quiche_engine_api.cpp
THREAD_UTILS_SRC = ../../engine/src/quiche_thread_utils.cpp
CLIENT_SRC = $(SRC_DIR)/client.cpp

# Object files
SERVER_OBJ = $(BUILD_DIR)/server.o
ENGINE_IMPL_OBJ = $(BUILD_DIR)/quiche_engine_impl.o
ENGINE_API_OBJ = $(BUILD_DIR)/quiche_engine_api.o
THREAD_UTILS_OBJ = $(BUILD_DIR)/quiche_thread_utils.o
CLIENT_OBJ = $(BUILD_DIR)/client.o
ENGINE_OBJS = $(ENGINE_IMPL_OBJ) $(ENGINE_API_OBJ) $(THREAD_UTILS_OBJ)

all: $(BUILD_DIR) $(CLIENT) $(SERVER)

client: $(BUILD_DIR) $(CLIENT)
server: $(BUILD_DIR) $(SERVER)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(CLIENT): $(CLIENT_OBJ) $(ENGINE_OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "Built $@ successfully"

$(SERVER): $(SERVER_OBJ)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	@echo "Built $@ successfully"

$(ENGINE_IMPL_OBJ): $(ENGINE_IMPL_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(ENGINE_API_OBJ): $(ENGINE_API_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(THREAD_UTILS_OBJ): $(THREAD_UTILS_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(CLIENT_OBJ): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(SERVER_OBJ): $(SERVER_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) $(CLIENT) $(SERVER)
	@echo "Cleaned"

.PHONY: all client server clean
