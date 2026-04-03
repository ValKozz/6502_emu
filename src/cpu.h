#ifndef CPU_H
#define CPU_H

#include <array>
#include <cinttypes>
#include <vector>
#include <stdbool.h>

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
#define DECB	0x08
#define BRKB	0x10
#define UNDF	0x20
#define OVRFL	0x40
#define NEG		0x80


class CPU
{
	bool running = false;
	u16 pc;
	// stack pointer $0100 to $01FF, points to the next free address; decrement on push
	u16 sp;
	u8 ac; 		// accumulator
	u8 x, y;
	u8 sta; 	// CPU status register
	std::vector<u8> memory;
	float cycle_lenght;

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

	void LDX_IMM();
	void LDX_ZPG();
	void LDX_ZPY();
	void LDX_ABS();
	void LDX_ABY();

	void LDY_IMM();
	void LDY_ZPG();
	void LDY_ZPX();
	void LDY_ABS();
	void LDY_ABX();

	void STA_ZPG();
	void STA_ZPX();
	void STA_ABS();
	void STA_ABX();
	void STA_ABY();
	void STA_XIN();
	void STA_INY();

	void STX_ZPG();
	void STX_ZPY();
	void STX_ABS();

	void STY_ZPG();
	void STY_ZPX();
	void STY_ABS();

	// Reg transfers Implued
	void TAX();
	void TAY();
	void TXA();
	void TYA();

	// Stack ops Implied
	void TSX();
	void TXS();
	void PHA(); // push ac to stack
	void PHP(); // push cpu sta to stack
	void PLA(); // pull ac from stack
	void PLP(); // cpu st from stack

	// Logical
	void AND_IMM();
	void AND_ZPG();
	void AND_ZPX();
	void AND_ABS();
	void AND_ABX();
	void AND_ABY();
	void AND_XIN();
	void AND_INY();

	void EOR_IMM(); // exclusive
	void EOR_ZPG();
	void EOR_ZPX();
	void EOR_ABS();
	void EOR_ABX();
	void EOR_ABY();
	void EOR_XIN();
	void EOR_INY();

	void ORA_IMM();	// inclusive
	void ORA_ZPG();
	void ORA_ZPX();
	void ORA_ABS();
	void ORA_ABX();
	void ORA_ABY();
	void ORA_XIN();
	void ORA_INY();

	void BIT_ABS(); // Bit test
	void BIT_ZPG(); 

	// Arithmetic
	void ADC_IMM(); // Add w/ carry
	void ADC_ZPG();
	void ADC_ZPX();
	void ADC_ABS();
	void ADC_ABX();
	void ADC_ABY();
	void ADC_XIN();
	void ADC_INY();

	void SBC_IMM();	// Substr w/ carry
	void SBC_ZPG();
	void SBC_ZPX();
	void SBC_ABS();
	void SBC_ABX();
	void SBC_ABY();
	void SBC_XIN();
	void SBC_INY();

	void CMP_IMM(); // compare ac
	void CMP_ZPG();
	void CMP_ZPX();
	void CMP_ABS();
	void CMP_ABX();
	void CMP_ABY();
	void CMP_XIN();
	void CMP_INY();

	void CPX_IMM(); 
	void CPX_ZPG();
	void CPX_ABS();

	void CPY_IMM(); 
	void CPY_ZPG();
	void CPY_ABS();

	// Increments and decrements
	void INC_ZPG();
	void INC_ZPX();
	void INC_ABS();
	void INC_ABX();

	void INX();
	void INY();

	void DEC_ZPG();
	void DEC_ZPX();
	void DEC_ABS();
	void DEC_ABX();

	void DEX();
	void DEY();

	// Shifts, Rotate instr use the CARRY flag bit to fill the void from the shift
	void ASL_A();
	void ASL_ZPG();
	void ASL_ZPX();
	void ASL_ABS();
	void ASL_ABX();

	void LSR_A();
	void LSR_ZPG();
	void LSR_ZPX();
	void LSR_ABS();
	void LSR_ABX();

	void ROL_A();
	void ROL_ZPG();
	void ROL_ZPX();
	void ROL_ABS();
	void ROL_ABX();

	void ROR_A();
	void ROR_ZPG();
	void ROR_ZPX();
	void ROR_ABS();
	void ROR_ABX();

	// JMP and Calls, JSR stores the pc onto the stack
	void JMP_ABS();	// JMP to another location (pc)
	void JMP_IND();

	void JSR(); // Jump to subroutine
	void RTS(); // Return from subroutine

	// Branch, moving PC if condition is met
	void BCC();
	void BCS();
	void BEQ();
	void BMI();
	void BNE();
	void BPL();
	void BVC();
	void BVS();

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
		// related to Addressing modes 
	using op_func = void (CPU::*)();
	static constexpr op_func ILL = &CPU::NOP;

	static constexpr std::array<op_func, 256> opcode_table = {
/*  0x0 			0x1 			 0x2    	   0x3    0x4 				0x5  		0x6			  0x7  0x8		  0x9			 0xA 		  0xB  0xC				0xD 		   0xE			  0XF*/
/*0x0*/ &CPU::BRK,    &CPU::ORA_XIN, ILL, 		   ILL, ILL, 		  	&CPU::ORA_ZPG, &CPU::ASL_ZPG, ILL, &CPU::PHP, &CPU::ORA_IMM, &CPU::ASL_A, ILL, ILL,		 		&CPU::ORA_ABS, &CPU::ASL_ABS, ILL,
/*0x1*/ &CPU::BPL,    &CPU::ORA_INY, ILL, 		   ILL, ILL, 		  	&CPU::ORA_ZPX, &CPU::ASL_ZPX, ILL, &CPU::CLC, &CPU::ORA_ABY, ILL,		  ILL, ILL, 			&CPU::ORA_ABX, &CPU::ASL_ABX, ILL,
/*0x2*/ &CPU::JSR,    &CPU::AND_XIN, ILL, 		   ILL, &CPU::BIT_ZPG, 	&CPU::AND_ZPG, &CPU::ROL_ZPG, ILL, &CPU::PLP, &CPU::AND_IMM, &CPU::ROL_A, ILL, &CPU::BIT_ABS, 	&CPU::AND_ABS, &CPU::ROL_ABS, ILL,
/*0x3*/ &CPU::BMI,    &CPU::AND_INY, ILL, 		   ILL, ILL,			&CPU::AND_ZPX, &CPU::ROL_ZPX, ILL, &CPU::SEC, &CPU::AND_ABY, ILL,		  ILL, ILL, 			&CPU::AND_ABX, &CPU::ROL_ABX, ILL,
/*0x4*/ &CPU::RTI,    &CPU::EOR_XIN, ILL, 		   ILL, ILL, 		  	&CPU::EOR_ZPG, &CPU::LSR_ZPG, ILL, &CPU::PHA, &CPU::EOR_IMM, &CPU::LSR_A, ILL, &CPU::JMP_ABS, 	&CPU::EOR_ABS, &CPU::LSR_ABS, ILL,
/*0x5*/ &CPU::BVC,    &CPU::EOR_INY, ILL, 		   ILL, ILL, 		  	&CPU::EOR_ZPX, &CPU::LSR_ZPX, ILL, &CPU::CLI, &CPU::EOR_ABY, ILL,		  ILL, ILL,				&CPU::EOR_ABX, &CPU::LSR_ABX, ILL,
/*0x6*/ &CPU::RTS,    &CPU::ADC_XIN, ILL, 		   ILL, ILL,			&CPU::ADC_ZPG, &CPU::ROR_ZPG, ILL, &CPU::PLA, &CPU::ADC_IMM, &CPU::ROR_A, ILL, &CPU::JMP_IND, 	&CPU::ADC_ABS, &CPU::ROR_ABS, ILL,
/*0x7*/ &CPU::BVS,    &CPU::ADC_INY, ILL, 		   ILL, ILL,			&CPU::ADC_ZPX, &CPU::ROR_ZPX, ILL, &CPU::SEI, &CPU::ADC_ABY, ILL,		  ILL, ILL, 			&CPU::ADC_ABX, &CPU::ROR_ABX, ILL,
/*0x8*/ ILL,		  &CPU::STA_XIN, ILL, 		   ILL, &CPU::STY_ZPG, 	&CPU::STA_ZPG, &CPU::STX_ZPG, ILL, &CPU::DEY, ILL,		     &CPU::TXA,   ILL, &CPU::STY_ABS,  	&CPU::STA_ABS, &CPU::STX_ABS, ILL,
/*0x9*/ &CPU::BCC,    &CPU::STA_INY, ILL, 		   ILL, &CPU::STY_ZPX, 	&CPU::STA_ZPX, &CPU::STX_ZPY, ILL, &CPU::TYA, &CPU::STA_ABY, &CPU::TXS,   ILL, ILL,				&CPU::STA_ABX, ILL,			  ILL,
/*0xA*/ &CPU::LDY_IMM,&CPU::LDA_XIN, &CPU::LDX_IMM,ILL, &CPU::LDY_ZPG, 	&CPU::LDA_ZPG, &CPU::LDX_ZPG, ILL, &CPU::TAY, &CPU::LDA_IMM, &CPU::TAX,   ILL, &CPU::LDY_ABS, 	&CPU::LDA_ABS, &CPU::LDX_ABS, ILL,
/*0xB*/	&CPU::BCS,	  &CPU::LDA_INY, ILL,		   ILL, &CPU::LDY_ZPX, 	&CPU::LDA_ZPX, &CPU::LDX_ZPY, ILL, &CPU::CLV, &CPU::LDA_ABX, &CPU::TSX,	  ILL, &CPU::LDY_ABX, 	&CPU::LDA_ABX, &CPU::LDX_ABY, ILL,
/*0xC*/ &CPU::CPY_IMM,&CPU::CMP_XIN, ILL,		   ILL, &CPU::CPY_ZPG, 	&CPU::CMP_ZPG, &CPU::DEC_ZPG, ILL, &CPU::INY, &CPU::CMP_IMM, &CPU::DEX,   ILL, &CPU::CPY_ABS, 	&CPU::CMP_ABS, &CPU::DEC_ABS, ILL,
/*0XD*/ &CPU::BNE,	  &CPU::CMP_INY, ILL,		   ILL, ILL,			&CPU::CMP_ZPX, &CPU::DEC_ZPX, ILL, &CPU::CLD, &CPU::CMP_ABY, ILL,		  ILL, ILL,				&CPU::CMP_ABX, &CPU::DEC_ABX, ILL,
/*0XE*/ &CPU::CPX_IMM,&CPU::SBC_XIN, ILL, 		   ILL, &CPU::CPX_ZPG, 	&CPU::SBC_ZPG, &CPU::INC_ZPG, ILL, &CPU::INX, &CPU::SBC_IMM, &CPU::NOP,   ILL, &CPU::CPX_ABS,  	&CPU::SBC_ABS, &CPU::INC_ABS, ILL,
/*0xF*/ &CPU::BEQ,    &CPU::SBC_INY, ILL,		   ILL, ILL,			&CPU::SBC_ZPX, &CPU::INC_ZPX, ILL, &CPU::SED, &CPU::SBC_ABY, ILL,		  ILL, ILL,				&CPU::SBC_ABX, &CPU::INC_ABX, ILL, 		 
	};
public:	
	CPU(uint8_t freq = 1); // used to reserve memory and set frequency in MHz
	bool load_prog();	
	void run();
};

#endif