# Quiche Engine Library
CXX = g++
AR = ar
CXXFLAGS = -Wall -Wextra -O2 -g -std=c++11
INCLUDES = -I./include -I./src -I../include -I/usr/local/include

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CXXFLAGS += -mmacosx-version-min=14.0
endif

SRC_DIR = src
BUILD_DIR = build
LIB_DIR = lib
TARGET = $(LIB_DIR)/libquiche_engine.a

# Source files
SRCS = $(SRC_DIR)/quiche_engine_impl.cpp \
       $(SRC_DIR)/quiche_engine_api.cpp \
       $(SRC_DIR)/quiche_thread_utils.cpp

# Object files
OBJS = $(BUILD_DIR)/quiche_engine_impl.o \
       $(BUILD_DIR)/quiche_engine_api.o \
       $(BUILD_DIR)/quiche_thread_utils.o

all: $(TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(LIB_DIR):
	@mkdir -p $(LIB_DIR)

$(TARGET): $(BUILD_DIR) $(LIB_DIR) $(OBJS)
	$(AR) rcs $@ $(OBJS)
	@echo "Built $@ successfully"

$(BUILD_DIR)/quiche_engine_impl.o: $(SRC_DIR)/quiche_engine_impl.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/quiche_engine_api.o: $(SRC_DIR)/quiche_engine_api.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/quiche_thread_utils.o: $(SRC_DIR)/quiche_thread_utils.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) $(LIB_DIR)
	@echo "Cleaned"

.PHONY: all clean
