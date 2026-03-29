#ifndef CPU_H
#define CPU_H

#include <cinttypes>
#include <vector>
#include <bool>

typedef uint8_t  u8;
typedef uint16_t u16;

/*
7  bit  0
---- ----
NV1B DIZC
|||| ||||
|||| |||+- Carry
|||| ||+-- Zero
|||| |+--- Interrupt Disable
|||| +---- Decimal
|||+------ (No CPU effect; see: the B flag)
||+------- (No CPU effect; always pushed as 1)
|+-------- Overflow
+--------- Negative

src: https://www.nesdev.org/wiki/Status_flags

Memory spans for 0x0000 to 0xFFFF
Last 6 bytes are reserved, 0xFFFA to 0xFFFF
0x0000 to 0x00FF is the zero page, used for sprecial addressing modes
0x01FF to 0x1FF is reserved for the stack.

*/

#define CARRY 	0x01
#define ZERO  	0x02
#define INTDIS	0x04
#define DECI	0x08
#define BRK		0x10
#define UNDF	0x20
#define OVRFL	0x40
#define NEG		0x80

class CPU
{
	bool running = false;
	u16 pc;
	// stack pointer $0100 to $01FF, points to the next free address; decrement on push
	u8 sp;
	u8 ac; 		// accumulator
	u8 x, y;
	u8 sta; 	// CPU status register
	std::vector<u8> memory;

	int cycle_lenght;

	void run_cycle();

	// helpers to set status register, eithre 0 or 1
	void set_CARRY(u8 status);
	void set_ZERO(u8 status);
	void set_INTDIS(u8 status);
	void set_DECI(u8 status);
	void set_BRK(u8 status);
	void set_OVRFL(u8 status);
	void set_NEG(u8 status);

	// Instruction Set
	// Load/Store operations
	void LDA(u16 addr); 
	void LDA(u8 addr); // 1 byte address zero page mode

	void LDX(u16 addr);
	void LDX(u8 addr); // 1 byte address zero page mode

	void LDY(u16 addr);
	void LDY(u8 addr); // 1 byte address zero page mode

	void STA(u16 addr);
	void STX(u16 addr);
	void STY(u16 addr);

	// Reg transfers
	void TAX();
	void TAY();
	void TXA();
	void TYA();

	// Stack ops
	void TSX(u16 addr);
	void TXS(u16 addr);
	void PHA(u16 addr); // push ac to stack
	void PHP(u16 addr); // push cpu sta to stack
	void PLA(u16 addr); // pull ac from stack
	void PLP(u16 addr); // cpu st from stack

	// Logical
	void AND();
	void EOR(); // exclusive
	void ORA();	// inclusive
	void BIT(); // Bit test

	// Arithmetic
	void ADC(); // Add w/ carry
	void SBC();	// Substr w/ carry
	void CMP(u16 addr); // compare ac
	void CPX(u16 addr); 
	void CPY(u16 addr); 

	// Increments and decrements
	void INC(u16 addr);
	void INX();
	void INY();
	void DEC(u16 addr);
	void DEX();
	void DEY();

	// Shifts, Rotate instr use the CARRY flag bit to fill the void from the shift
	void ASL(u16 addr);
	void ASD_ac();
	void LSR();
	void ROL();
	void ROR();

	// JMP and Calls, JSR stores the pc onto the stack
	void JMP(u16 addr);	// JMP to another location (pc)
	void JSR(u16 addr); // Jump to subroutine
	void RTS(); // Return from subroutine

	// Branch, moving PC if condition is met
	void BCC(u16 addr);
	void BCS(u16 addr);
	void BEQ(u16 addr);
	void BMI(u16 addr);
	void BNE(u16 addr);
	void BPL(u16 addr);
	void BVC(u16 addr);
	void BVS(u16 addr);

	// status flag change
	void CLC();
	void CLD();
	void CLI();
	void CLV();
	void SEC();
	void SED();
	void SEI();

	// Sys functions
	void BRK();
	void NOP();
	void RTI();

	void fetch();
	void execute();

public:	
	CPU(uint8_t freq = 1); // used to reserve memory and set frequency in MHz
	bool loadProg();	
	void run();
};

#endif