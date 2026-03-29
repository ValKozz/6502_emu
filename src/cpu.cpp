#include "cpu.h"
#include <fstream>
#include <iostream>


CPU::CPU(uint8_t freq = 1) { 
	if (freq > 3) freq = 3;
	cycle_lenght = (1 / 1000000) * freq  // MHz
	sp =  = 0x01FF;
	sta = 0;
	x = 0;
	y = 0;
	ac = 0;
	// Init memory and initialize to 0
	memory.resize(0xFFFF);
	running = true;
}

