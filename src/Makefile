CFLAGS=-std=c++11 -D DEBUG -g
BUILD_DIR=../build
OBJ_DIR=../obj

all: $(BUILD_DIR)/6502_emu

$(BUILD_DIR)/6502_emu: $(OBJ_DIR)/cpu.o $(OBJ_DIR)/main.o
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CFLAGS) $(OBJ_DIR)/cpu.o $(OBJ_DIR)/main.o -o $(BUILD_DIR)/6502_emu

$(OBJ_DIR)/main.o: main.cpp
	$(CXX) $(CFLAGS) -c main.cpp -o $(OBJ_DIR)/main.o

$(OBJ_DIR)/cpu.o: cpu.h cpu.cpp
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CFLAGS) -c cpu.cpp -o $(OBJ_DIR)/cpu.o

clean: 
	rm -r $(OBJ_DIR)
	rm -r $(BUILD_DIR)
