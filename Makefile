CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
LDFLAGS  = -lpthread

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:src/%.cpp=build/%.o)
BIN = build/server

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p build
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build
