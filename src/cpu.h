#ifndef CPU_H
#define CPU_H

#include <array>
#include <cinttypes>
#include <vector>
#include <bool>

typedef uint8_t  u8;
typedef uint16_t u16;

// Table to store function pointers, to evade the double switch case
// related to Addressing modes 
using op_func = void (CPU::*)();
// OP, cycles
using OP = std::pair(op_func, u8);
#define ILL (&CPU::NOP, 2)


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
#define DEC 	0x08
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
	float cycle_lenght;

	static const std::array<OP, 256> opcode_table = {
/*  0x0 			0x1 			 0x2    0x3   				    0x4 					0x5  				0x6				 	0x7   		0x8			0x9				0xA 			 0xB  0xC				 0xD 				  0xE					0XF*/
/*0x0*/ (&CPU::BRK, 7),    (&CPU::ORA_XIN, 6), ILL, 			    ILL, ILL, 			  	 (&CPU::ORA_ZPG, 2), (&CPU::ASL_ZPG, 5), ILL, (&CPU::PHP, 3), (&CPU::ORA_IMM, 2), (&CPU::ASL_A, 2), ILL, ILL,		 		 (&CPU::ORA_ABS, 2), (&CPU::ASL_ABS, 6), ILL,
/*0x1*/ (&CPU::BPL, 2),    (&CPU::ORA_INY, 5), ILL, 			    ILL, ILL, 			  	 (&CPU::ORA_ZPX, 4), (&CPU::ASL_ZPX, 6), ILL, (&CPU::CLC, 2), (&CPU::ORA_ABY, 4), ILL,			   	ILL, ILL, 				 (&CPU::ORA_ABX, 4), (&CPU::ASL_ABX, 7), ILL,
/*0x2*/ (&CPU::JSR, 6),    (&CPU::AND_XIN, 6), ILL, 			    ILL, (&CPU::BIT_ZPG,3),  (&CPU::AND_ZPG, 3), (&CPU::ROL_ZPG, 5), ILL, (&CPU::PLP, 4), (&CPU::AND_IMM, 2), (&CPU::ROL_A, 2), ILL, (&CPU::BIT_ABS, 4), (&CPU::AND_ABS, 4), (&CPU::ROL_ABS, 6), ILL,
/*0x3*/ (&CPU::BMI, 2),    (&CPU::AND_INY, 5), ILL, 			    ILL, ILL,				 (&CPU::AND_ZPX, 4), (&CPU::ROL_ZPX, 6), ILL, (&CPU::SEC, 2), (&CPU::AND_ABY, 4), ILL,			    ILL, ILL, 				 (&CPU::AND_ABX, 4), (&CPU::ROL_ABX, 6), ILL,
/*0x4*/ (&CPU::RTI, 6),    (&CPU::EOR_XIN, 6), ILL, 			    ILL, ILL, 			  	 (&CPU::EOR_ZPG, 3), (&CPU::LSR_ZPG, 5), ILL, (&CPU::PHA, 3), (&CPU::EOR_IMM, 2), (&CPU::LSR_A, 2), ILL, (&CPU::JMP_ABS, 3), (&CPU::EOR_ABS, 4), (&CPU::LSR_ABS, 6), ILL,
/*0x5*/ (&CPU::BVC, 2),    (&CPU::EOR_INY, 5), ILL, 			    ILL, ILL, 			  	 (&CPU::EOR_ZPX, 4), (&CPU::LSR_ZPX, 6), ILL, (&CPU::CLI, 2), (&CPU::EOR_ABY, 4), ILL,				ILL, ILL,				 (&CPU::EOR_ABX, 4), (&CPU::LSR_ABX, 7), ILL,
/*0x6*/ (&CPU::RTS, 6),    (&CPU::ADC_XIN, 6), ILL, 			    ILL, ILL,				 (&CPU::ADC_ZPG, 3), (&CPU::ROR_ZPG, 5), ILL, (&CPU::PLA, 4), (&CPU::ADC_IMM, 2), (&CPU::ROR_A, 2), ILL, (&CPU::JMP_IND, 5), (&CPU::ADC_ABS, 4), (&CPU::ROR_ABS, 6), ILL,
/*0x7*/ (&CPU::BVS, 2),    (&CPU::ADC_INY, 5), ILL, 			    ILL, ILL,				 (&CPU::ADC_ZPX, 4), (&CPU::ROR_ZPX, 6), ILL, (&CPU::SEI, 2), (&CPU::ADC_ABY, 4), ILL, 			 	ILL, ILL, 				 (&CPU::ADC_ABX, 4), (&CPU::ROR_ABX, 7), ILL,
/*0x8*/ ILL,			   (&CPU::STA_XIN, 6), ILL, 			 	ILL, (&CPU::STY_ZPG,3),  (&CPU::STA_ZPG, 3), (&CPU::STX_ZPG, 3), ILL, (&CPU::DEY, 2), ILL,			      (&CPU::TXA, 2),   ILL, (&CPU::STY_ABS, 4), (&CPU::STA_ABS, 4), (&CPU::STX_ABS, 4), ILL,
/*0x9*/ (&CPU::BCC, 2),    (&CPU::STA_INY, 6), ILL, 			 	ILL, (&CPU::STY_ZPX, 4), (&CPU::STA_ZPX, 4), (&CPU::STX_ZPY, 4), ILL, (&CPU::TYA, 2), (&CPU::STA_ABY, 5), (&CPU::TXS, 2),   ILL, ILL,				 (&CPU::STA_ABX, 5), ILL,				 ILL,
/*0xA*/ (&CPU::LDY_IMM, 2),(&CPU::LDA_XIN, 6), (&CPU::LDX_IMM, 2),  ILL, (&CPU::LDY_ZPG, 3), (&CPU::LDA_ZPG, 3), (&CPU::LDX_ZPG, 3), ILL, (&CPU::TAY, 2), (&CPU::LDA_IMM, 2), (&CPU::TAX, 2),   ILL, (&CPU::LDY_ABS, 4), (&CPU::LDA_ABS, 4), (&CPU::LDX_ABS, 4), ILL,
/*0xB*/	(&CPU::BCS, 2),	   (&CPU::LDA_INY, 5), ILL,					ILL, (&CPU::LDY_ZPX, 4), (&CPU::LDA_ZPX, 4), (&CPU::LDX_ZPY, 4), ILL, (&CPU::CLV, 2), (&CPU::LDA_ABX, 4), (&CPU::TSX, 2),	ILL, (&CPU::LDY_ABX, 4), (&CPU::LDA_ABX, 4), (&CPU::LDX_ABY, 4), ILL,
/*0xC*/ (&CPU::CPY_IMM, 2),(&CPU::CMP_XIN, 6), ILL,					ILL, (&CPU::CPY_ZPG, 3), (&CPU::CMP_ZPG, 3), (&CPU::DEC_ZPG, 5), ILL, (&CPU::INY, 2), (&CPU::CMP_IMM, 2), (&CPU::DEX, 2),   ILL, (&CPU::CPY_ABS, 4), (&CPU::CMP_ABS, 4), (&CPU::DEC_ABS, 6), ILL,
/*0XD*/ (&CPU::BNE, 2),	   (&CPU::CMP_INY, 5), ILL,					ILL, ILL,				 (&CPU::CMP_ZPX, 4), (&CPU::DEX_ZPX, 6), ILL, (&CPU::CLD, 2), (&CPU::CMP_ABY, 4), ILL,				ILL, ILL,				 (&CPU::CMP_ABY, 4), (&CPU::DEC_ABX, 7), ILL,
/*0XE*/ (&CPU::CPX_IMM, 2),(&CPU::SBC_XIN, 6), ILL, 				ILL, (&CPU::CPX_ZPG, 3), (&CPU::SBC_ZPG, 3), (&CPU::INC_ZPG, 5), ILL, (&CPU::INX, 2), (&CPU::SBC_IMM, 2), (&CPU::NOP, 2), 	ILL, (&CPU::CPX_ABS, 4), (&CPU::SBC_ABS, 4), (&CPU::INC_ABS, 6), ILL,
/*0xF*/ (&CPU::BEQ, 2),    (&CPU::SBC_INY, 5), ILL,					ILL, ILL,				 (&CPU::SBC_ZPX, 4), (&CPU::INC_ZPX, 6), ILL, (&CPU::SED, 2), (&CPU::SBC_ABY, 4), ILL,				ILL, ILL,				 (&CPU::SBC_ABX, 4), (&CPU::INC_ABX, 7), ILL, 		 
	}

	void run_cycle(int cycles);

	// helpers to set status register, either 0 or 1
	void set_status(u8 bit);

	void push_stack(u16 value);
	u16 pop_stack();

	// helpers to access memeory
	u8 read_byte(u16 addr);
	void write_byte(u16 addr, u8 data);
	u8 get_zero_page(u8 addr);
	u8 get_absolute(u16 addr);
	u8 get_indirect(u16 addr);
	u8 get_indirect_zero(u8 addr);
	
	// Instruction Set TODO MUST ADD ALL MODES
	// Load/Store operations
	void LDA_IMM(); 
	void LDA_ZPG();
	void LDA_ZPX();
	void LDA_ZPY();
	void LDA_ABS();
	void LDA_ABX();
	void LDA_ABY();
	void LDA_XIN(); // indexed indirect, add x to addr
	void LDA_INY(); // indirect indexed


	void LDX();
	void LDY();

	void STA();
	void STX();
	void STY();

	// Reg transfers
	void TAX();
	void TAY();
	void TXA();
	void TYA();

	// Stack ops
	void TSX();
	void TXS();
	void PHA(); // push ac to stack
	void PHP(); // push cpu sta to stack
	void PLA(); // pull ac from stack
	void PLP(); // cpu st from stack

	// Logical
	void AND();
	void EOR(); // exclusive
	void ORA();	// inclusive
	void BIT(); // Bit test

	// Arithmetic
	void ADC(); // Add w/ carry
	void SBC();	// Substr w/ carry
	void CMP(); // compare ac
	void CPX(); 
	void CPY(); 

	// Increments and decrements
	void INC();
	void XIN();
	void INY();
	void DEC();
	void DEX();
	void DEY();

	// Shifts, Rotate instr use the CARRY flag bit to fill the void from the shift
	void ASL();
	void LSR();
	void ROL();
	void ROR();

	// JMP and Calls, JSR stores the pc onto the stack
	void JMP();	// JMP to another location (pc)
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
	bool load_prog();	
	void run();
};

#endif