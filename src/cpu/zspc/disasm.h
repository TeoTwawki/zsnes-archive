// SNES SPC-700 disassembler

#ifndef SPC_DISASM_H
#define SPC_DISASM_H

#ifdef __cplusplus
	extern "C" {
#endif

// Length of instruction (1, 2, or 3 bytes)
int spc_disasm_len( int opcode );

// Disassemble instruction into output string and return length of instruction.
// opcode = mem [addr], data = mem [addr + 1], data2 = mem [addr + 2]
enum { spc_disasm_max = 32 }; // maximum length of output string
int spc_disasm( unsigned addr, int opcode, int data, int data2, char* out );

// Returns template form of opcode without any values filled in
const char* spc_disasm_form( int opcode );


#ifdef __cplusplus
	}
#endif

#endif
