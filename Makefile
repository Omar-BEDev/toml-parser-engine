# Compiler settings
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

# Build directories
SRC_DIR  := src
OBJ_DIR  := obj
BIN_DIR  := bin
TARGET   := $(BIN_DIR)/main

SOURCES  := $(shell find $(SRC_DIR) -name '*.cpp')


OBJECTS  := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))

INC_FLAGS := $(addprefix -I,$(sort $(dir $(SOURCES))))


.PHONY: all clean

all: $(TARGET)


$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC_FLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)