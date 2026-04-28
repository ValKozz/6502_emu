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

#define IMM_OP(reg, op) do {			\
    (reg) op read_byte(++pc);           \
    status_on_transfer((reg));          \
    run_cycles(2);                      \
} while (0)

#define ZPG_OP(reg, op) do {	\
	u8 addr = read_byte(++pc);	\
	(reg) op read_byte(addr);	\
	status_on_transfer((reg));	\
	run_cycles(3);				\
} while (0)

#define ZPA_OP(reg, op, added_reg) do {		\
	u8 addr = read_byte(++pc);				\
	(reg) op read_byte(addr + (added_reg));	\
	status_on_transfer((reg));				\
	run_cycles(4);							\
} while (0)

#define ABS_OP(reg, op) do {							\
	u16 addr = read_byte(++pc) << 8 | read_byte(pc);	\
	(reg) op read_byte(addr);							\
	status_on_transfer((reg));							\
	run_cycles(4);										\
} while (0)

#define ABA_OP(reg, op, added_reg) do {					\
	u8 cycles = 4;										\
	u16 addr = read_byte(++pc) << 8 | read_byte(pc);	\
	if ((addr & 0xFF00) > ((addr+x) & 0xFF00)) cycles++;\
	(reg) op read_byte(addr + (added_reg));				\
	status_on_transfer((reg));							\
	run_cycles(cycles);									\
} while (0)

#define XIN_OP(reg, op) do {							\
	u8 pt = read_byte(++pc);							\
	u8 pt_lo_addr = pt + x;								\
	u8 pt_hi_addr = pt_lo_addr + 1;						\
	u16 addr = ((u16)pt_hi_addr << 8) | pt_lo_addr;		\
	(reg) op read_byte(addr);							\
	status_on_transfer((reg));							\
	run_cycles(6);										\
} while (0)

#define INY_OP(reg, op) do {										\
	u8 cycles = 5;													\
	u8 pt = read_byte(++pc);										\
	u16 base_addr = (u16)(read_byte(pt+1) << 8) | read_byte(pt);	\
	u16 addr = read_byte(base_addr + y);							\
	if ((base_addr & 0xFF00) != (addr & 0xFF00)) cycles++;			\
	(reg) op read_byte(addr);										\
	status_on_transfer((reg));										\
	run_cycles(cycles);												\
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
	sp = 0x00;
	set_status(INTDIS_POS, 1); // set interupt disable bit in status register
	run_cycles(1);
	// Cycle 1-3
	// Do nothing?
	run_cycles(3);

	u8 lo = read_byte(0xFFFC);
	u8 hi = read_byte(0xFFFD);
	pc = (hi << 8 | lo) - 1;
	
	DEBMSG("Set PC to", pc);
	run_cycles(3); // +1 for fetch
	fetch();
}


void CPU::run_cycles(int cycles) {
	for (int i = 0; i < cycles; i++) {
		sleep(cycle_lenght);
	}
}

void CPU::push_stack(u8 value) {
	// get actual memory location
	u16 mem_loc = 0x0100 & sp--;
	write_byte(mem_loc, value);
	if (sp == 0xFF) WARN("OVERFLOW DETECTED, STACK POINTER", sp);
}

u8 CPU::pop_stack() {
	// get actual memory location
	u16 mem_loc = 0x0100 & sp++;
	u8 popped = read_byte(mem_loc);
 	if (sp == 0x00) WARN("UNDERFLOW DETECTED, STACK POINTER", sp);
 	return popped;
}

u8 CPU::peek_stack() {
	u8 value = read_byte(0x100 & sp);
	return value;	
}

// helpers to set status register, either 0 or 1
void CPU::set_status(u8 bit_pos, u8 value) {
	sta |= (bit_pos << value);
}

void CPU::status_on_transfer(u8 reg) {
	if (reg & (0x1 << 7)) set_status(NEG_POS, 1);
	else set_status(NEG_POS, 0);

	if (reg == 0) set_status(ZERO_POS, 1);
	else set_status(ZERO_POS, 0);
}

// helpers to access memeory
void CPU::write_byte(u16 addr, u8 data) {
	if (addr < MEMSIZE) {
		memory[addr] = data;
	}
	else {
		fprintf(stderr, "Attempted to write out of memory bounds @%04X\n", addr);
		dump_mem();
		running = 0;
	}
}

u8 CPU::read_byte(u16 addr) {
	if (addr < MEMSIZE+1) {
		return memory[addr];
	}
	else {
		fprintf(stderr, "Attempted to read out of memory bounds @%04X\n", addr);
		dump_mem();
		running = 0;
		return 1;
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
void CPU::fetch() {
	pc+=1;
	// temp for testing
	DEBMSG("Fetched", memory[pc] << 8 | memory[pc]);
}

// TODO
void CPU::execute(){}

// TODO
// Load/Store operations
void CPU::LDA_IMM() {IMM_OP(ac, =);}
void CPU::LDA_ZPG() {ZPG_OP(ac, =);}
void CPU::LDA_ZPX() {ZPA_OP(ac, =, x);}
void CPU::LDA_ABS() {ABS_OP(ac, =);}
void CPU::LDA_ABX() {ABA_OP(ac, =, x);}
void CPU::LDA_ABY() {ABA_OP(ac, =, y);}
void CPU::LDA_XIN() {XIN_OP(ac, =);} 
void CPU::LDA_INY() {INY_OP(ac, =);} 

void CPU::LDX_IMM() {IMM_OP(x, =);}
void CPU::LDX_ZPG() {ZPG_OP(x, =);}
void CPU::LDX_ZPY() {ZPA_OP(x, =, y);}
void CPU::LDX_ABS() {ABS_OP(x, =);}
void CPU::LDX_ABY() {ABA_OP(x, =, y);}


void CPU::LDY_IMM() {IMM_OP(y, =);}
void CPU::LDY_ZPG() {ZPG_OP(y, =);}
void CPU::LDY_ZPX() {ZPA_OP(y, =, x);}
void CPU::LDY_ABS() {ABS_OP(y, =);}
void CPU::LDY_ABX() {ABA_OP(y, =, x);}

void CPU::STA_ZPG() {
	u8 addr = read_byte(++pc);
	write_byte(addr, ac);
	run_cycles(3);
}

void CPU::STA_ZPX() {
	// let it wrap around on it's own?
	u8 addr = read_byte(++pc) + x;
	write_byte(addr, ac);
	run_cycles(4);
}

void CPU::STA_ABS() {
	u16 addr = read_byte(++pc) << 8| read_byte(pc);
	write_byte(addr, ac);
	run_cycles(4); 
}

void CPU::STA_ABX() {
	u16 addr = read_byte(++pc) << 8 | read_byte(pc);
	write_byte(addr + x, ac);
	run_cycles(5);
}

void CPU::STA_ABY() {
	u16 addr = ++pc << 8| ++pc;
	write_byte(addr + y, ac);
	run_cycles(5);
}

void CPU::STA_XIN() {
	u8 pt = read_byte(++pc);
	u8 pt_lo_addr = pt + x; 
	u8 pt_hi_addr = pt_lo_addr + 1;
	u16 addr = ((u16)pt_hi_addr << 8) | pt_lo_addr;

	write_byte(addr, ac);
	run_cycles(6);
}

void CPU::STA_INY() {
	u8 pt = read_byte(++pc);
	u16 base_addr = (u16)(read_byte(pt+1) << 8) | read_byte(pt);
	u16 addr = read_byte(base_addr + y);
	
	write_byte(addr, ac);

	run_cycles(6);
}


void CPU::STX_ZPG() {
	u8 addr = read_byte(++pc);
	write_byte(addr, x);
	run_cycles(3);
}

void CPU::STX_ZPY() {
	u8 addr = read_byte(++pc);
	write_byte(addr, x);
	run_cycles(4);
}

void CPU::STX_ABS() {
	u16 addr = read_byte(++pc) << 8| read_byte(pc);
	write_byte(addr, x);
	run_cycles(4); 
}


void CPU::STY_ZPG() {
	u8 addr = read_byte(++pc);
	write_byte(addr, y);
	run_cycles(3);
}

void CPU::STY_ZPX() {
	u8 addr = read_byte(++pc);
	write_byte(addr, y);
	run_cycles(4);
}

void CPU::STY_ABS() {
	u16 addr = read_byte(++pc) << 8| read_byte(pc);
	write_byte(addr, y);
	run_cycles(4); 
}


// // Reg transfers Implied
void CPU::TAX() {
	x = ac;
	status_on_transfer(x);
	run_cycles(2);
}

void CPU::TAY() {
	y = ac;
	status_on_transfer(y);
	run_cycles(2);
}

void CPU::TXA() {
	ac = x;	
	status_on_transfer(ac);
	run_cycles(2);
}

void CPU::TYA() {
	ac = y;
	status_on_transfer(ac);
	run_cycles(2);
}

// // Stack ops Implied
void CPU::TSX() {
	x = sp;
	status_on_transfer(x);
	run_cycles(2);
}

void CPU::TXS() {
	sp = x;
	run_cycles(2);
}

void CPU::PHA() {
	push_stack(ac);
	run_cycles(3);
} 

void CPU::PHP() {
	push_stack(sta);
	run_cycles(3);
} 

void CPU::PLA() {
	ac = pop_stack();
	status_on_transfer(ac);
	run_cycles(4);
} 

void CPU::PLP() {
	sta = pop_stack();
	run_cycles(4);
} 

// // Logical
void CPU::AND_IMM() {IMM_OP(ac, &=);}
void CPU::AND_ZPG() {ZPG_OP(ac, &=);}
void CPU::AND_ZPX() {ZPA_OP(ac, &=, x);}
void CPU::AND_ABS() {ABS_OP(ac, &=);}
void CPU::AND_ABX() {ABA_OP(ac, &=, x);}
void CPU::AND_ABY() {ABA_OP(ac, &=, y);}
void CPU::AND_XIN() {XIN_OP(ac, &=);}
void CPU::AND_INY() {INY_OP(ac, &=);}

void CPU::EOR_IMM() {IMM_OP(ac, ^=);}
void CPU::EOR_ZPG() {ZPG_OP(ac, ^=);}
void CPU::EOR_ZPX() {ZPA_OP(ac, ^=, x);}
void CPU::EOR_ABS() {ABS_OP(ac, ^=);}
void CPU::EOR_ABX() {ABA_OP(ac, ^=, x);}
void CPU::EOR_ABY() {ABA_OP(ac, ^=, y);}
void CPU::EOR_XIN() {XIN_OP(ac, ^=);}
void CPU::EOR_INY() {INY_OP(ac, ^=);}

void CPU::ORA_IMM() {IMM_OP(ac, |=);}
void CPU::ORA_ZPG() {ZPG_OP(ac, |=);}
void CPU::ORA_ZPX() {ZPA_OP(ac, |=, x);}
void CPU::ORA_ABS() {ABS_OP(ac, |=);}
void CPU::ORA_ABX() {ABA_OP(ac, |=, x);}
void CPU::ORA_ABY() {ABA_OP(ac, |=, y);}
void CPU::ORA_XIN() {XIN_OP(ac, |=);}
void CPU::ORA_INY() {INY_OP(ac, |=);}


/*
bits 7 and 6 of operand are transfered to N and V
Z is set by result of A AND operand
*/
void CPU::BIT_ZPG() {
	u8 addr = read_byte(++pc);
	u8 oper = read_byte(addr);

	u8 b7 = oper >> 7;
	u8 b6 = (oper >> 6) & 0x01;
	b7 ? set_status(NEG_POS, 1) : set_status(NEG_POS, 0);
	b6 ? set_status(OVRFL_POS, 1) : set_status(OVRFL_POS, 0);

	u8 res = ac & oper;
	res ? set_status(ZERO_POS, 1) : set_status(ZERO_POS, 0);
	run_cycles(3);
}

void CPU::BIT_ABS() {
	u16 addr = read_byte(++pc) << 8 | read_byte(pc);
	u8 oper = read_byte(addr);

	u8 b7 = oper >> 7;
	u8 b6 = (oper >> 6) & 0x1;
	b7 ? set_status(NEG_POS, 1) : set_status(NEG_POS, 0);
	b6 ? set_status(OVRFL_POS, 1) : set_status(OVRFL_POS, 0);

	u8 res = ac & oper;
	res ? set_status(ZERO_POS, 1) : set_status(ZERO_POS, 0);
	run_cycles(3);
	run_cycles(4);
}

// Arithmetic
void CPU::ADC_IMM() {
	// TODO
}

// void CPU::ADC_ZPG() {

// }

// void CPU::ADC_ZPX() {

// }

// void CPU::ADC_ABS() {

// }

// void CPU::ADC_ABX() {

// }

// void CPU::ADC_ABY() {

// }

// void CPU::ADC_XIN() {

// }

// void CPU::ADC_INY() {

// }


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
