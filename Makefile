# Compiler settings
CXX      := g++

CXXFLAGS := -std=c++17 -Wall -g -Wextra -O2 -MMD -MP

# Build directories
SRC_DIR  := src
OBJ_DIR  := obj
BIN_DIR  := bin

ifeq ($(OS),Windows_NT)
    EXT := .exe
else
    EXT :=
endif

TARGET      := $(BIN_DIR)/tpe$(EXT)
TEST_TARGET := $(BIN_DIR)/tests$(EXT)

SOURCES      := $(wildcard $(SRC_DIR)/*/*.cpp) $(wildcard $(SRC_DIR)/*.cpp)
TEST_SOURCES := $(wildcard test/*.cpp)

OBJECTS      := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))
TEST_OBJECTS := $(patsubst test/%.cpp, $(OBJ_DIR)/test/%.o, $(TEST_SOURCES))
TEST_SUPPORT_OBJECTS := $(filter-out $(OBJ_DIR)/main.o, $(OBJECTS))

INC_FLAGS := $(addprefix -I,$(sort $(dir $(SOURCES))))

DEPS := $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)

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


-include $(DEPS)