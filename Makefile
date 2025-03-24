# Compiler
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -I./websocketpp
LDFLAGS = -lcurl -lboost_system -lboost_thread -lpthread -lssl -lcrypto -lsimdjson  

# Directories
SRC_DIR = src
BUILD_DIR = build

# Files
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))

# Output
TARGET = $(BUILD_DIR)/main

# Build
$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)  # Link necessary libraries

# Compile sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build directory
clean:
	rm -rf $(BUILD_DIR)

# Run program
run: $(TARGET)
	$(TARGET)
