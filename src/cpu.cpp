#include "cpu.h"
#include <fstream>
#include <iostream>
#include <unistd.h>

CPU::CPU(uint8_t freq) { 
	if (freq > 3) freq = 3;
	 // 1 MHz = 1 000 000 Hz 
	cycle_lenght = 1 / (1000000 * freq);
	
	sp = 0x00FF;
	sta = 0;
	x = 0;
	y = 0;
	ac = 0;
	// Init memory and initialize to 0
	memory.resize(0x10000);
	running = true;
}

void CPU::run_cycle(int cycles) {
	for (int i = 0; i < cycles; i++) {
		sleep(cycle_lenght);
	}
}

void CPU::push_stack(u16 value) {
	memory[sp--] = value;
	if (sp < 0x0100) {
		sp = 0x01FF;
		fprintf(stderr, "OVERFLOW DETECTED STACK POINTER: %4X\n", sp);
	}
}

u16 CPU::pop_stack() {
	u16 popped = memory[sp++];
 	if (sp > 0x01FF) {
 		sp = 0x0100;
		fprintf(stderr, "UNDERFLOW DETECTED STACK POINTER: %4X\n", sp);
 	}
 	return popped;
}

