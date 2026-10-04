# Compiler settings
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -g -Wextra -O2

# Build directories
SRC_DIR  := src
OBJ_DIR  := obj
BIN_DIR  := bin
TARGET   := $(BIN_DIR)/main
TEST_TARGET := $(BIN_DIR)/tests

SOURCES  := $(shell find $(SRC_DIR) -name '*.cpp')
TEST_SOURCES := $(shell find test -name '*.cpp')


OBJECTS  := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))
TEST_OBJECTS := $(patsubst test/%.cpp, $(OBJ_DIR)/test/%.o, $(TEST_SOURCES))
TEST_SUPPORT_OBJECTS := $(filter-out $(OBJ_DIR)/main.o, $(OBJECTS))

INC_FLAGS := $(addprefix -I,$(sort $(dir $(SOURCES))))


.PHONY: all test clean

all: $(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)


$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@

$(TEST_TARGET): $(TEST_OBJECTS) $(TEST_SUPPORT_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -lgtest -lgtest_main -pthread -o $@


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC_FLAGS) -c $< -o $@

$(OBJ_DIR)/test/%.o: test/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC_FLAGS) -I. -DarenaSize=test_arenaSize -Darena=test_arena -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
