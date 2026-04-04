#include "cpu.h"
#include <fstream>
#include <iostream>
#include <unistd.h>

#define MAX_PROG_SIZE 0xFFFF - 0xC000

#ifndef DEBUG
#define DEBUG 0
#else 
#define DEBUG 1
#endif

#define DEBMSG(mess, opcode) do {							\
		if (DEBUG) printf("%s : %04X\n", (mess), (opcode)); \
	} while (0)

#define DEBRD(mess, data, opcode) do {						\
	if (DEBUG) printf("%s : %4X @ %04X\n")					\
} while (0)

#define WARN(mess, opcode) do {												\
		if (DEBUG) fprintf(stderr, "WARN: %s : %04X\n", (mess), (opcode)); 	\
	} while (0)

CPU::CPU(uint8_t freq) { 
	if (freq > 3) freq = 3;
	 // 1 MHz = 1 000 000 Hz 
	cycle_lenght = 1 / (1000000 * freq);
	sta = 0;
	x = 0;
	y = 0;
	ac = 0;
	// Init memory and initialize to 0
	memory.resize(MEMSIZE+1);
	running = true;
}

bool CPU::load_prog(std::vector<u8> prog, int vec_size) {
	if (vec_size >= MAX_PROG_SIZE) {
		fprintf(stderr, "Program too large to fit into memory!\n");
		return false;
	}
	int prog_start = (MEMSIZE+1) - vec_size;
	for (int i = 0; i < vec_size; i++) memory[i + prog_start] = prog[i];

	return true;
}

void CPU::dump_mem() {
	#if DEBUG
	printf("REGS:\nPC: @%04X (%04X) SP: %04X\nX: %04X Y: %04X AC: %04X\nSTATUS: %04X\n\n",pc, memory[pc], sp, x, y, ac, sta);

	std::ofstream outfile("mem_dmp.bin", std::ios::binary);
	if (!outfile.is_open()) {
		fprintf(stderr, "Error writing mem dump to file!\n");
		return;
	}

	for (int i = 0; i < 0x10000; i++) {
		outfile.write((char*)&(memory[i]), sizeof(u8));
	}
	outfile.close();
	#else
	return
	#endif
}

/*
CPU intitializes in 7 cycles
	Cycle 0: 	SP is set to $00 and PC is set to FFFC, Interupt flag is set to 1 (disabled)
	Cycles 1-3: CPU peforms 3 read cycles at stack $0100, &01FF, and $01FE
	Cycles 4-5: CPU reads lo $FFFC and hi $FFFD and loads it into PC
	Cycles 6:   First instruction set is fetched and executed
	
*/
void CPU::reset() {
	DEBMSG("Running Initialization routine PC set to", 0xFFFC);
	// Cycle 0
	sp = 0x0100;	// sp is 16 bit to make addressing easier witm memory[sp], checks for overflow are done on push/pop
	set_status(INTDIS); // set interupt disable bit in status register
	run_cycles(1);
	// Cycle 1-3
	// Do nothing?
	run_cycles(3);

	u8 lo = read_byte(0xFFFC);
	u8 hi = read_byte(0xFFFD);
	pc = hi << 8 | lo;
	
	DEBMSG("Set PC to", pc);
	run_cycles(2);
	fetch();
}


void CPU::run_cycles(int cycles) {
	for (int i = 0; i < cycles; i++) {
		sleep(cycle_lenght);
	}
}

void CPU::push_stack(u16 value) {
	memory[sp--] = value;
	if (sp < 0x0100) {
		sp = 0x01FF;
		WARN("OVERFLOW DETECTED, STACK POINTER", sp);
	}
}

u16 CPU::pop_stack() {
	u16 popped = memory[sp++];
 	if (sp > 0x01FF) {
 		sp = 0x0100;
		WARN("UNDERFLOW DETECTED, STACK POINTER", sp);
 	}
 	return popped;
}
	// helpers to set status register, either 0 or 1
void CPU::set_status(u8 bit) {
		sta |= bit;
	}

// TODO 
void CPU::fetch() {
	// temp for testing
	DEBMSG("Fetched", memory[pc] << 8 | memory[++pc]);
	run_cycles(1);
}

// TODO
void CPU::execute(){}

// helpers to access memeory
void CPU::write_byte(u16 addr, u8 data) {
	if (addr < MEMSIZE) {
		memory[addr] = data;
	}
	else {
		fprintf(stderr, "Attempted to write out of memory bounds @%04X\n", addr);
	}
}

u8 CPU::read_byte(u16 addr) {
	if (addr < MEMSIZE+1) {
		return memory[addr];
	}
	else {
		fprintf(stderr, "Attempted to read out of memory bounds @%04X\n", addr);
		return 0;
	}
}

u8 CPU::get_indirect(u16 addr) {
	if (addr < MEMSIZE+1) {
		u16 data_addr = memory[addr];
	 	if (data_addr < MEMSIZE+1) return memory[data_addr];
	 	else fprintf(stderr, "Attempted to read out of memory bounds after INDIRECT @%04X = %04X\n", addr, data_addr);
		return 0;
	}
	else {
		fprintf(stderr, "Attempted to read out of memory bounds before INDIRECT @%04X\n", addr);
		return 0;
	}
}

// public run TODO
void CPU::run() {
	reset();
	// testing
	for (int i = 0; i < 5; i++) fetch();
	
	dump_mem();
}


// TODO
// Load/Store operations
// void CPU::LDA_IMM() {
// 	run_cycles(2);
// }

// void CPU::LDA_ZPG() {
// 	run_cycles(3);
// }

// void CPU::LDA_ZPX() {
// 	run_cycles(4);
// }

// void CPU::LDA_ABS() {
// 	run_cycles(4);
// }

// void CPU::LDA_ABX() {
// 	// TODO check if page crossed and add an extra cycle
// 	u8 cycles = 4;
	
// 	run_cycles(cycles);
// }

// void CPU::LDA_ABY() {
// 	// TODO check if page crossed and add an extra cycle
// 	u8 cycles = 4;
	
// 	run_cycles(cycles);

// }

// void CPU::LDA_XIN() { // indexed indirect, add x to addr
// 	run_cycles(6);
// } 

// void CPU::LDA_INY() { // indirect indexed
// 	// TODO check if page crossed and add an extra cycle
// 	u8 cycles = 5;

// 	run_cycles(cycles);
// } 

// void CPU::LDX_IMM();
// void CPU::LDX_ZPG();
// void CPU::LDX_ZPY();
// void CPU::LDX_ABS();
// void CPU::LDX_ABY();

// void CPU::LDY_IMM();
// void CPU::LDY_ZPG();
// void CPU::LDY_ZPX();
// void CPU::LDY_ABS();
// void CPU::LDY_ABX();

// void CPU::STA_ZPG();
// void CPU::STA_ZPX();
// void CPU::STA_ABS();
// void CPU::STA_ABX();
// void CPU::STA_ABY();
// void CPU::STA_XIN();
// void CPU::STA_INY();

// void CPU::STX_ZPG();
// void CPU::STX_ZPY();
// void CPU::STX_ABS();

// void CPU::STY_ZPG();
// void CPU::STY_ZPX();
// void CPU::STY_ABS();

// // Reg transfers Implued
// void CPU::TAX();
// void CPU::TAY();
// void CPU::TXA();
// void CPU::TYA();

// // Stack ops Implied
// void CPU::TSX();
// void CPU::TXS();
// void CPU::PHA(); // push ac to stack
// void CPU::PHP(); // push cpu sta to stack
// void CPU::PLA(); // pull ac from stack
// void CPU::PLP(); // cpu st from stack

// // Logical
// void CPU::AND_IMM();
// void CPU::AND_ZPG();
// void CPU::AND_ZPX();
// void CPU::AND_ABS();
// void CPU::AND_ABX();
// void CPU::AND_ABY();
// void CPU::AND_XIN();
// void CPU::AND_INY();

// void CPU::EOR_IMM(); // exclusive
// void CPU::EOR_ZPG();
// void CPU::EOR_ZPX();
// void CPU::EOR_ABS();
// void CPU::EOR_ABX();
// void CPU::EOR_ABY();
// void CPU::EOR_XIN();
// void CPU::EOR_INY();

// void CPU::ORA_IMM();	// inclusive
// void CPU::ORA_ZPG();
// void CPU::ORA_ZPX();
// void CPU::ORA_ABS();
// void CPU::ORA_ABX();
// void CPU::ORA_ABY();
// void CPU::ORA_XIN();
// void CPU::ORA_INY();

// void CPU::BIT_ABS(); // Bit test
// void CPU::BIT_ZPG(); 

// // Arithmetic
// void CPU::ADC_IMM(); // Add w/ carry
// void CPU::ADC_ZPG();
// void CPU::ADC_ZPX();
// void CPU::ADC_ABS();
// void CPU::ADC_ABX();
// void CPU::ADC_ABY();
// void CPU::ADC_XIN();
// void CPU::ADC_INY();

// void CPU::SBC_IMM();	// Substr w/ carry
// void CPU::SBC_ZPG();
// void CPU::SBC_ZPX();
// void CPU::SBC_ABS();
// void CPU::SBC_ABX();
// void CPU::SBC_ABY();
// void CPU::SBC_XIN();
// void CPU::SBC_INY();

// void CPU::CMP_IMM(); // compare ac
// void CPU::CMP_ZPG();
// void CPU::CMP_ZPX();
// void CPU::CMP_ABS();
// void CPU::CMP_ABX();
// void CPU::CMP_ABY();
// void CPU::CMP_XIN();
// void CPU::CMP_INY();

// void CPU::CPX_IMM(); 
// void CPU::CPX_ZPG();
// void CPU::CPX_ABS();

// void CPU::CPY_IMM(); 
// void CPU::CPY_ZPG();
// void CPU::CPY_ABS();

// // Increments and decrements
// void CPU::INC_ZPG();
// void CPU::INC_ZPX();
// void CPU::INC_ABS();
// void CPU::INC_ABX();

// void CPU::INX();
// void CPU::INY();

// void CPU::DEC_ZPG();
// void CPU::DEC_ZPX();
// void CPU::DEC_ABS();
// void CPU::DEC_ABX();

// void CPU::DEX();
// void CPU::DEY();

// // Shifts, Rotate instr use the CARRY flag bit to fill the void from the shift
// void CPU::ASL_A();
// void CPU::ASL_ZPG();
// void CPU::ASL_ZPX();
// void CPU::ASL_ABS();
// void CPU::ASL_ABX();

// void CPU::LSR_A();
// void CPU::LSR_ZPG();
// void CPU::LSR_ZPX();
// void CPU::LSR_ABS();
// void CPU::LSR_ABX();

// void CPU::ROL_A();
// void CPU::ROL_ZPG();
// void CPU::ROL_ZPX();
// void CPU::ROL_ABS();
// void CPU::ROL_ABX();

// void CPU::ROR_A();
// void CPU::ROR_ZPG();
// void CPU::ROR_ZPX();
// void CPU::ROR_ABS();
// void CPU::ROR_ABX();

// // JMP and Calls, JSR stores the pc onto the stack
// void CPU::JMP_ABS();	// JMP to another location (pc)
// void CPU::JMP_IND();

// void CPU::JSR(); // Jump to subroutine
// void CPU::RTS(); // Return from subroutine

// // Branch, moving PC if condition is met
// void CPU::BCC();
// void CPU::BCS();
// void CPU::BEQ();
// void CPU::BMI();
// void CPU::BNE();
// void CPU::BPL();
// void CPU::BVC();
// void CPU::BVS();

// // status flag change
// void CPU::CLC();
// void CPU::CLD();
// void CPU::CLI();
// void CPU::CLV();
// void CPU::SEC();
// void CPU::SED();
// void CPU::SEI();

// // Sys functions
// void CPU::BRK();
// void CPU::NOP();
// void CPU::RTI();
