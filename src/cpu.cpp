#include "cpu.h"
#include <csetjmp>
#include <fstream>
#include <iostream>

#include <unistd.h>

#define MAX_PROG_SIZE 0xFFFF

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
	u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);	\
	(reg) op read_byte(addr);							\
	status_on_transfer((reg));							\
	run_cycles(4);										\
} while (0)

#define ABA_OP(reg, op, added_reg) do {					\
	u8 cycles = 4;										\
	u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);	\
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

#define INY_OP(reg, op) do {										    \
	u8 cycles = 5;													    \
	u8 pt_lo_addr = read_byte(++pc);								    \
	u8 pt_hi_addr = read_byte(++pc);                                    \
	u16 base_addr = (u16)(read_byte(pt_hi_addr) << 8) | read_byte(pt_lo_addr);\
	u16 addr = base_addr + y;						            	    \
	if ((base_addr & 0xFF00) != (addr & 0xFF00)) cycles++;			    \
	(reg) op read_byte(addr);										    \
	status_on_transfer((reg));										    \
	run_cycles(cycles);												    \
} while (0)

#define BCOMP_STA(bit_pos, cmp_value) do {                                          \
    u8 cycles = 2;                                                                  \
    u8 bit_value = get_status((bit_pos));                                           \
    ++pc;                                                                           \
    if ((bit_value) == (cmp_value)) {                                               \
        cycles += 1;                                                                \
        u16 new_pc = pc + read_byte(pc);                                            \
        if ((pc & 0xFF00) != (new_pc & 0xFF00)) cycles++;                           \
        pc = new_pc;                                                                \
    }                                                                               \
    run_cycles(cycles);                                                             \
} while (0)

#define CMP_IMM_OP(reg) do {                            \
    u8 temp = (reg) - read_byte(++pc);                  \
    status_on_cmp(temp);                                \
    run_cycles(2);                                      \
} while(0)

#define CMP_ABS_OP(reg, added_reg) do {                             \
    u8 cycles = 4;                                                  \
    u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);       \
   	if ((addr & 0xFF00) > ((addr+(added_reg)) & 0xFF00)) cycles++;  \
    u8 temp = (reg) - read_byte(addr + (added_reg));                \
    status_on_cmp(temp);                                            \
    run_cycles(cycles);                                             \
} while (0)

#define CMP_ZPR_OP(reg, added_reg) do {                 \
    u8 addr = read_byte(++pc) + (added_reg);            \
    u8 temp = (reg) - read_byte(addr | 0x00FF);         \
    status_on_cmp(temp);                                \
    run_cycles(3);                                      \
} while (0)

// Body for the ADC instructions
#define ADC_BODY(operand, cycles) do {                  \
    /* collect bits 7, to check for overflow */         \
    u8 ac_b7 = (ac >> 7) & 0x1;                         \
    u8 oper_b7 = ((operand) >> 7) & 0x1;                \
                                                        \
    u16 res = ac + (operand) + get_status(CARRY_POS);   \
                                                        \
    u8 res_b7 = (res >> 7) & 0x1;                       \
    u8 new_carry = (res >> 8) & 0x1;                    \
    set_status(CARRY_POS, new_carry);                   \
                                                        \
    if ((ac_b7 != oper_b7) && (res_b7 != ac_b7)){       \
        set_status(OVRFL_POS, 1);                       \
    }                                                   \
    else set_status(OVRFL_POS, 0);                      \
    ac = (res & 0xFF);                                  \
    status_on_transfer(ac);                             \
    run_cycles((cycles));                               \
} while (0)

#define SBC_BODY(operand, cycles) do {                          \
    /* collect bits 7, to check for overflow */                 \
    u8 ac_b7 = (ac >> 7) & 0x1;                                 \
    u8 oper_b7 = ((operand) >> 7) & 0x1;                        \
                                                                \
    u16 res = ac - (operand) - (get_status(CARRY_POS) ? 0 : 1); \
                                                                \
    u8 res_b7 = (res >> 7) & 0x1;                               \
    set_status(CARRY_POS, (res < 0x100) ? 0 : 1);               \
                                                                \
    if ((ac_b7 != oper_b7) && (res_b7 != ac_b7)){               \
        set_status(OVRFL_POS, 1);                               \
    }                                                           \
    else set_status(OVRFL_POS, 0);                              \
    ac = (res & 0xFF);                                          \
    status_on_transfer(ac);                                     \
    run_cycles((cycles));                                       \
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

// helpers to set and get status register, either 0 or 1
void CPU::set_status(u8 bit_pos, u8 value) {
	sta |= (bit_pos << value);
}

u8 CPU::get_status(u8 bit_pos) {
    // get only the bit in the position and mask it
    u8 value = (sta >> bit_pos) & 0x1;
    return value;
}

void CPU::status_on_transfer(u8 reg) {
	if (reg & (0x1 << 7)) set_status(NEG_POS, 1);
	else set_status(NEG_POS, 0);

	if (reg == 0) set_status(ZERO_POS, 1);
	else set_status(ZERO_POS, 0);
}

void CPU::status_on_cmp(u8 value) {
    if (value != 0) {
        set_status(ZERO_POS, 0);

        u8 b7 = (value >> 7) & 0x1;
        if (b7) {
            set_status(NEG_POS, 1);
            set_status(CARRY_POS, 0);
        }
        else {
            set_status(NEG_POS, 0);
            set_status(CARRY_POS, 1);
        }
    }
    else {
        set_status(ZERO_POS, 1);
    }
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
    // should wrap around
	return memory[addr];
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
	DEBMSG("Fetched", memory[pc]);
	execute();
}

// TODO
void CPU::execute() {
    u8 op_type = memory[pc];
    void (CPU::*OP)(void) = opcode_table[op_type];
    (this->*OP)();
}

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
	u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
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
	u8 pt_lo_addr = read_byte(++pc);
	u8 pt_hi_addr = read_byte(++pc);
	u16 base_addr = (u16)(read_byte(pt_hi_addr) << 8) | read_byte(pt_lo_addr);
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


// Reg transfers Implied
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

// Stack ops Implied
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
	set_status(BRKB_POS, 1);
	run_cycles(3);
}

void CPU::PLA() {
	ac = pop_stack();
	status_on_transfer(ac);
	run_cycles(4);
}

void CPU::PLP() {
	sta = pop_stack();
	// BRK flag is always masked and cleared on restore
	set_status(BRKB_POS, 0);
	run_cycles(4);
}

// Logical
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
	u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
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
    u8 operand = read_byte(++pc);
    ADC_BODY(operand, 2);
}

void CPU::ADC_ZPG() {
    u8 addr = read_byte(++pc);
    u8 operand = read_byte(addr);
    ADC_BODY(operand, 3);
}

void CPU::ADC_ZPX() {
    u8 addr = read_byte(++pc);
    u8 operand = read_byte(addr + x);
    ADC_BODY(operand, 4);
}

void CPU::ADC_ABS() {
    u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    u8 operand = read_byte(addr);
    ADC_BODY(operand, 4);
}

void CPU::ADC_ABX() {
    u8 cycles = 4;
    u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    if ((addr & 0xFF00) > ((addr+x) & 0xFF00)) cycles++;
    u8 operand = read_byte(addr+x);
    ADC_BODY(operand, cycles);
}

void CPU::ADC_ABY() {
    u8 cycles = 4;
    u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    if ((addr & 0xFF00) > ((addr+y) & 0xFF00)) cycles++;
    u8 operand = read_byte(addr+y);
    ADC_BODY(operand, cycles);
}

void CPU::ADC_XIN() {
    u8 pt = read_byte(++pc);
    u8 pt_lo_addr = pt+x;
    u8 pt_hi_addr = pt_lo_addr + 1;
    u16 addr = ((u16)pt_hi_addr << 8) | pt_lo_addr;
    u8 operand = read_byte(addr);
    ADC_BODY(operand, 6);
}

void CPU::ADC_INY() {
    u8 cycles = 5;
	u8 pt_lo_addr = read_byte(++pc);
	u8 pt_hi_addr = read_byte(++pc);
	u16 base_addr = (u16)(read_byte(pt_hi_addr) << 8) | read_byte(pt_lo_addr);
	u16 addr = base_addr + y;
	if ((base_addr & 0xFF00) != (addr & 0xFF00)) cycles++;
	u8 operand = read_byte(addr);
	ADC_BODY(operand, cycles);
}

// Substr w/ carry
void CPU::SBC_IMM() {
    u8 operand = read_byte(++pc);
    SBC_BODY(operand, 2);
}
void CPU::SBC_ZPG() {
    u8 addr = read_byte(++pc);
    u8 operand = read_byte(addr);
    SBC_BODY(operand, 3);
}
void CPU::SBC_ZPX() {
    u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    u8 operand = read_byte(addr);
    SBC_BODY(operand, 4);
}
void CPU::SBC_ABS() {
    u8 cycles = 4;
    u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    if ((addr & 0xFF00) > ((addr+x) & 0xFF00)) cycles++;
    u8 operand = read_byte(addr+x);
    SBC_BODY(operand, cycles);
}
void CPU::SBC_ABX() {
    u8 cycles = 4;
    u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    if ((addr & 0xFF00) > ((addr+x) & 0xFF00)) cycles++;
    u8 operand = read_byte(addr+x);
    SBC_BODY(operand, cycles);
}
void CPU::SBC_ABY() {
    u8 cycles = 4;
    u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    if ((addr & 0xFF00) > ((addr+y) & 0xFF00)) cycles++;
    u8 operand = read_byte(addr+y);
    SBC_BODY(operand, cycles);
}
void CPU::SBC_XIN() {
    u8 pt = read_byte(++pc);
    u8 pt_lo_addr = pt+x;
    u8 pt_hi_addr = pt_lo_addr + 1;
    u16 addr = ((u16)pt_hi_addr << 8) | pt_lo_addr;
    u8 operand = read_byte(addr);
    SBC_BODY(operand, 6);
}
void CPU::SBC_INY() {
    u8 cycles = 5;
	u8 pt_lo_addr = read_byte(++pc);
	u8 pt_hi_addr = read_byte(++pc);
	u16 base_addr = (u16)(read_byte(pt_hi_addr) << 8) | read_byte(pt_lo_addr);
	u16 addr = base_addr + y;
	if ((base_addr & 0xFF00) != (addr & 0xFF00)) cycles++;
	u8 operand = read_byte(addr);
	SBC_BODY(operand, cycles);
}

void CPU::CMP_IMM() {CMP_IMM_OP(ac);}
void CPU::CMP_ZPG() {CMP_ZPR_OP(ac, 0);}
void CPU::CMP_ZPX() {CMP_ZPR_OP(ac, x);}
void CPU::CMP_ABS() {CMP_ABS_OP(ac, 0);}
void CPU::CMP_ABX() {CMP_ABS_OP(ac, x);}
void CPU::CMP_ABY() {CMP_ABS_OP(ac, y);}

void CPU::CMP_XIN() {
    u8 pt = read_byte(++pc);
    u8 pt_lo_addr = pt+x;
    u8 pt_hi_addr = pt_lo_addr + 1;

    u16 addr = ((u16)pt_hi_addr << 8) | pt_lo_addr;
    u8 temp = ac - read_byte(addr);
    if (temp > 0) set_status(CARRY_POS, 1);
    else set_status(CARRY_POS, 0);

    if (temp == 0) set_status(ZERO_POS, 1);
    else set_status(ZERO_POS, 0);

    u8 b7 = (temp >> 7) & 0x1;
    if (b7) set_status(NEG_POS, 1);
    else set_status(NEG_POS, 0);
    run_cycles(6);
}

void CPU::CMP_INY() {
    u8 cycles = 5;
    u8 pt_lo_addr = read_byte(++pc);
	u8 pt_hi_addr = read_byte(++pc);
	u16 base_addr = (u16)(read_byte(pt_hi_addr) << 8) | read_byte(pt_lo_addr);
	u16 addr = base_addr + y;

	if ((base_addr & 0xFF00) != (addr & 0xFF00)) cycles++;
	u8 temp = ac - read_byte(addr);

	if (temp > 0) set_status(CARRY_POS, 1);
    else set_status(CARRY_POS, 0);

    if (temp == 0) set_status(ZERO_POS, 1);
    else set_status(ZERO_POS, 0);

    u8 b7 = (temp >> 7) & 0x1;
    if (b7) set_status(NEG_POS, 1);
    else set_status(NEG_POS, 0);

	run_cycles(cycles);
}

void CPU::CPX_IMM() {CMP_IMM_OP(x);}
void CPU::CPX_ZPG() {CMP_ZPR_OP(x, 0);}
void CPU::CPX_ABS() {CMP_ABS_OP(x, 0);}

void CPU::CPY_IMM() {CMP_IMM_OP(y);}
void CPU::CPY_ZPG() {CMP_ZPR_OP(y, 0);}
void CPU::CPY_ABS() {CMP_ABS_OP(y, 0);}


// Increments and decrements
void CPU::INC_ZPG() {
    u8 addr = ++pc;
    memory[addr] += 1;
    status_on_transfer(memory[addr]);
    run_cycles(5);
}

void CPU::INC_ZPX() {
    u8 addr = ++pc + x;
    memory[addr] += 1;
    status_on_transfer(memory[addr]);
    run_cycles(6);
}

void CPU::INC_ABS() {
    u16 addr = read_byte(++pc) | (u16)read_byte(++pc) << 8;
    memory[addr] += 1;
    status_on_transfer(memory[addr]);
    run_cycles(6);
}

void CPU::INC_ABX() {
    u16 addr = read_byte(++pc) | (u16)read_byte(++pc) << 8;
    addr += x;
    memory[addr] += 1;
    status_on_transfer(memory[addr]);
    run_cycles(7);
}

void CPU::INX() {
    x++;
    status_on_transfer(x);
    run_cycles(2);
}

void CPU::INY() {
    y++;
    status_on_transfer(y);
    run_cycles(2);
}

void CPU::DEC_ZPG() {
    u8 addr = ++pc;
    memory[addr] -= 1;
    status_on_transfer(memory[addr]);
    run_cycles(5);
}

void CPU::DEC_ZPX() {
    u8 addr = ++pc + x;
    memory[addr] -= 1;
    status_on_transfer(memory[addr]);
    run_cycles(6);
}

void CPU::DEC_ABS() {
    u16 addr = read_byte(++pc) | (u16)read_byte(++pc) << 8;
    memory[addr] -= 1;
    status_on_transfer(memory[addr]);
    run_cycles(6);
}

void CPU::DEC_ABX() {
    u16 addr = read_byte(++pc) | (u16)read_byte(++pc) << 8;
    addr += x;
    memory[addr] -= 1;
    status_on_transfer(memory[addr]);
    run_cycles(7);
}

void CPU::DEX() {
    x--;
    status_on_transfer(x);
    run_cycles(2);
}

void CPU::DEY() {
    y--;
    status_on_transfer(y);
    run_cycles(2);
}

// Shifts, Rotate instr use the CARRY flag bit to fill the void from the shift
void CPU::ASL_A() {
    u8 b7 = (ac >> 7) & 0x1;
    ac <<= 1;
    status_on_transfer(ac);
    set_status(CARRY_POS, b7);
    run_cycles(2);
}

void CPU::ASL_ZPG() {
    u8 addr = read_byte(++pc);
    u8 b7 = (memory[addr] >> 7) & 0x1;
    memory[addr] <<= 1;
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b7);
    run_cycles(5);
}

void CPU::ASL_ZPX() {
    u8 addr = read_byte(++pc) + x;
    u8 b7 = (memory[addr] >> 7) & 0x1;
    memory[addr] <<= 1;
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b7);
    run_cycles(6);
}

void CPU::ASL_ABS() {
   	u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    u8 b7 = (memory[addr] >> 7) & 0x1;
    memory[addr] <<= 1;
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b7);
    run_cycles(6);
}

void CPU::ASL_ABX() {
   	u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    addr += x;
    u8 b7 = (memory[addr] >> 7) & 0x1;
    memory[addr] <<= 1;
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b7);
    run_cycles(7);
}

void CPU::LSR_A() {
    u8 b0 = ac & 0x1;
    ac >>= 1;
    status_on_transfer(ac);
    set_status(CARRY_POS, b0);
    run_cycles(2);
}

void CPU::LSR_ZPG() {
    u8 addr = read_byte(++pc);
    u8 b0 = memory[addr] & 0x1;
    memory[addr] >>= 1;
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b0);
    run_cycles(5);
}

void CPU::LSR_ZPX() {
    u8 addr = read_byte(++pc) + x;
    u8 b0 = memory[addr] & 0x1;
    memory[addr] >>= 1;
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b0);
    run_cycles(6);
}

void CPU::LSR_ABS() {
   	u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    u8 b0 = memory[addr] & 0x1;
    memory[addr] >>= 1;
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b0);
    run_cycles(6);
}

void CPU::LSR_ABX() {
   	u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    addr += x;
    u8 b0 = (memory[addr] >> 7) & 0x1;
    memory[addr] >>= 1;
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b0);
    run_cycles(7);
}

void CPU::ROL_A() {
    u8 old_carry = get_status(CARRY_POS);
    u8 b7 = (ac >> 7) & 0x1;
    ac <<= 1;
    ac |= old_carry;
    status_on_transfer(ac);
    set_status(CARRY_POS, b7);
   run_cycles(2);
}

void CPU::ROL_ZPG() {
    u8 old_carry = get_status(CARRY_POS);
    u8 addr = read_byte(++pc);
    u8 b7 = (memory[addr] >> 7) & 0x1;
    memory[addr] <<= 1;
    memory[addr] |= old_carry;
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b7);
   run_cycles(5);
}

void CPU::ROL_ZPX() {
    u8 old_carry = get_status(CARRY_POS);
    u8 addr = read_byte(++pc) + x;
    u8 b7 = (memory[addr] >> 7) & 0x1;
    memory[addr] <<= 1;
    memory[addr] |= old_carry;
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b7);
   run_cycles(6);
}

void CPU::ROL_ABS() {
    u8 old_carry = get_status(CARRY_POS);
   	u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    u8 b0 = memory[addr] & 0x1;
    memory[addr] >>= 1;
    memory[addr] |= old_carry;
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b0);
   run_cycles(6);
}

void CPU::ROL_ABX() {
    u8 old_carry = get_status(CARRY_POS);
   	u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    addr += x;
    u8 b7 = (memory[addr] >> 7) & 0x1;
    memory[addr] <<= 1;
    memory[addr] |= old_carry;
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b7);
   run_cycles(7);
}

void CPU::ROR_A() {
    u8 old_carry = get_status(CARRY_POS);
    u8 b0 = ac & 0x1;
    ac >>= 1;
    ac |= (old_carry << 7);
    status_on_transfer(ac);
    set_status(CARRY_POS, b0);
   run_cycles(2);
}

void CPU::ROR_ZPG() {
    u8 old_carry = get_status(CARRY_POS);
    u8 addr = read_byte(++pc);
    u8 b0 = memory[addr] & 0x1;
    memory[addr] >>= 1;
    memory[addr] |= (old_carry << 7);
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b0);
    run_cycles(5);
}

void CPU::ROR_ZPX() {
    u8 old_carry = get_status(CARRY_POS);
    u8 addr = read_byte(++pc) + x;
    u8 b0 = memory[addr] & 0x1;
    memory[addr] >>= 1;
    memory[addr] |= (old_carry << 7);
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b0);
    run_cycles(6);
}

void CPU::ROR_ABS() {
    u8 old_carry = get_status(CARRY_POS);
    u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    u8 b0 = memory[addr] & 0x1;
    memory[addr] >>= 1;
    memory[addr] |= (old_carry << 7);
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b0);
    run_cycles(6);
}

void CPU::ROR_ABX() {
    u8 old_carry = get_status(CARRY_POS);
    u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    addr += x;
    u8 b0 = memory[addr] & 0x1;
    memory[addr] >>= 1;
    memory[addr] |= (old_carry << 7);
    status_on_transfer(memory[addr]);
    set_status(CARRY_POS, b0);
    run_cycles(7);
}

// JMP and Calls, JSR stores the pc onto the stack
void CPU::JMP_ABS() {
    u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    pc = addr;
    run_cycles(3);
}

// on original 6502, it does not fetch correctly when hitting a page boundary
// instead most sig byte is taken from XX00
void CPU::JMP_IND() {
    u8 pt_lo_addr = read_byte(++pc);
	u8 pt_hi_addr = read_byte(++pc);
	u16 base_addr = (u16)(read_byte(pt_hi_addr) << 8) | read_byte(pt_lo_addr);
	// adding one to the base address should implement the incorrect handling, of LSB is 0xXXFF + 1 on a u8 should wrap around to 0xXX00
	u16 addr = (u16)(read_byte(base_addr+1) << 8) | read_byte(base_addr);
	pc = addr;
	run_cycles(5);
}

// push return point - 1 to stack, put pc on address of subroutine
void CPU::JSR() {
    u16 addr = read_byte(++pc) | ((u16)read_byte(++pc) << 8);
    // push pc's current position, as +1 would be the return
    push_stack(pc);
    pc = addr;
    run_cycles(6);
}

// Return from subroutine
void CPU::RTS() {
    pc = pop_stack();
    run_cycles(6);
}

// Branch, moving PC if condition is met
void CPU::BCC() {BCOMP_STA(CARRY_POS, 0);}
void CPU::BCS() {BCOMP_STA(CARRY_POS, 1);}
void CPU::BEQ() {BCOMP_STA(ZERO_POS, 1);}
void CPU::BMI() {BCOMP_STA(NEG_POS, 1);}
void CPU::BNE() {BCOMP_STA(ZERO_POS, 0);}
void CPU::BPL() {BCOMP_STA(NEG_POS, 0);}
void CPU::BVC() {BCOMP_STA(OVRFL_POS, 0);}
void CPU::BVS() {BCOMP_STA(OVRFL_POS, 1);}

// status flag change
void CPU::CLC() {
    set_status(CARRY_POS, 0);
    run_cycles(2);
}

void CPU::CLD() {
    set_status(DECB_POS, 0);
    run_cycles(2);
}

void CPU::CLI() {
    set_status(INTDIS_POS, 0);
    run_cycles(2);
}

void CPU::CLV() {
    set_status(OVRFL_POS, 0);
    run_cycles(2);
}

void CPU::SEC() {
    set_status(CARRY_POS, 1);
    run_cycles(2);
}

void CPU::SED() {
    set_status(DECB_POS, 0);
    run_cycles(2);
}

void CPU::SEI() {
    set_status(INTDIS_POS, 0);
    run_cycles(2);
}

// Sys functions
// Store status to stack and put pc to FFFE, set BRK in status to 1
void CPU::BRK() {
    // push pc to stack +2 as BRK is a 2 byte instruction with it's padding
    push_stack(pc+2);
    push_stack(sta);
    set_status(BRKB_POS, 1);
    pc = 0xFFFE;
    run_cycles(7);
}

void CPU::NOP() {
    run_cycles(2);
    ++pc;
}
// Pull CPU status from stack after interupt and restore pc
void CPU::RTI() {
    u8 restored_status = pop_stack();
    sta = restored_status;
    pc = pop_stack();
    run_cycles(6);
}
