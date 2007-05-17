// http://www.slack.net/~ant/

#include "disasm.h"

#include <stdio.h>

/* Copyright (C) 2005-2007 by Shay Green. Permission is hereby granted, free of
charge, to any person obtaining a copy of this software module and associated
documentation files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the
following conditions: The above copyright notice and this permission notice
shall be included in all copies or substantial portions of the Software. THE
SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

static char const op_lens [256] =
{ //0 1 2 3 4 5 6 7 8 9 A B C D E F
    1,1,2,3,2,3,1,2,2,3,3,2,3,1,3,1,// 0
    2,1,2,3,2,3,3,2,3,1,2,2,1,1,3,3,// 1
    1,1,2,3,2,3,1,2,2,3,3,2,3,1,3,2,// 2
    2,1,2,3,2,3,3,2,3,1,2,2,1,1,2,3,// 3
    1,1,2,3,2,3,1,2,2,3,3,2,3,1,3,2,// 4
    2,1,2,3,2,3,3,2,3,1,2,2,1,1,3,3,// 5
    1,1,2,3,2,3,1,2,2,3,3,2,3,1,3,1,// 6
    2,1,2,3,2,3,3,2,3,1,2,2,1,1,2,1,// 7
    1,1,2,3,2,3,1,2,2,3,3,2,3,2,1,3,// 8
    2,1,2,3,2,3,3,2,3,1,2,2,1,1,1,1,// 9
    1,1,2,3,2,3,1,2,2,3,3,2,3,2,1,1,// A
    2,1,2,3,2,3,3,2,3,1,2,2,1,1,1,1,// B
    1,1,2,3,2,3,1,2,2,3,3,2,3,2,1,1,// C
    2,1,2,3,2,3,3,2,2,2,2,2,1,1,3,1,// D
    1,1,2,3,2,3,1,2,2,3,3,2,3,1,1,1,// E
    2,1,2,3,2,3,3,2,2,2,3,2,1,1,2,1 // F
};

int spc_disasm_len( int opcode ) { return op_lens [opcode]; }

static const char op_names [0x100] [16] =
{
"NOP",            "TCALL 0",        "SET1  d.0",      "BBS   d.0, r",
"OR    A, d",     "OR    A, !a",    "OR    A, (X)",   "OR    A, [d+X]",
"OR    A, #i",    "OR    dd, ds",   "OR1   C, m.b",   "ASL   d",
"ASL   !a",       "PUSH  PSW",      "TSET1 !a",       "BRK",
"BPL   r",        "TCALL 1",        "CLR1  d.0",      "BBC   d.0, r",
"OR    A, d+X",   "OR    A, !a+X",  "OR    A, !a+Y",  "OR    A, [d]+Y",
"OR    d, #i",    "OR    (X), (Y)", "DECW  d",        "ASL   d+X",
"ASL   A",        "DEC   X",        "CMP   X, !a",    "JMP   [!a+X]",
"CLRP",           "TCALL 2",        "SET1  d.1",      "BBS   d.1, r",
"AND   A, d",     "AND   A, !a",    "AND   A, (X)",   "AND   A, [d+X]",
"AND   A, #i",    "AND   dd, ds",   "OR1   C, /m.b",  "ROL   d",
"ROL   !a",       "PUSH  A",        "CBNE  d, r",     "BRA   r",
"BMI   r",        "TCALL 3",        "CLR1  d.1",      "BBC   d.1, r",
"AND   A, d+X",   "AND   A, !a+X",  "AND   A, !a+Y",  "AND   A, [d]+Y",
"AND   d, #i",    "AND   (X), (Y)", "INCW  d",        "ROL   d+X",
"ROL   A",        "INC   X",        "CMP   X, d",     "CALL  !a",
"SETP",           "TCALL 4",        "SET1  d.2",      "BBS   d.2, r",
"EOR   A, d",     "EOR   A, !a",    "EOR   A, (X)",   "EOR   A, [d+X]",
"EOR   A, #i",    "EOR   dd, ds",   "AND1  C, m.b",   "LSR   d",
"LSR   !a",       "PUSH  X",        "TCLR1 !a",       "PCALL u",
"BVC   r",        "TCALL 5",        "CLR1  d.2",      "BBC   d.2, r",
"EOR   A, d+X",   "EOR   A, !a+X",  "EOR   A, !a+Y",  "EOR   A, [d]+Y",
"EOR   d, #i",    "EOR   (X), (Y)", "CMPW  YA, d",    "LSR   d+X",
"LSR   A",        "MOV   X, A",     "CMP   Y, !a",    "JMP   !a",
"CLRC",           "TCALL 6",        "SET1  d.3",      "BBS   d.3, r",
"CMP   A, d",     "CMP   A, !a",    "CMP   A, (X)",   "CMP   A, [d+X]",
"CMP   A, #i",    "CMP   dd, ds",   "AND1  C, /m.b",  "ROR   d",
"ROR   !a",       "PUSH  Y",        "DBNZ  d, r",     "RET",
"BVS   r",        "TCALL 7",        "CLR1  d.3",      "BBC   d.3, r",
"CMP   A, d+X",   "CMP   A, !a+X",  "CMP   A, !a+Y",  "CMP   A, [d]+Y",
"CMP   d, #i",    "CMP   (X), (Y)", "ADDW  YA, d",    "ROR   d+X",
"ROR   A",        "MOV   A, X",     "CMP   Y, d",     "RET1",
"SETC",           "TCALL 8",        "SET1  d.4",      "BBS   d.4, r",
"ADC   A, d",     "ADC   A, !a",    "ADC   A, (X)",   "ADC   A, [d+X]",
"ADC   A, #i",    "ADC   dd, ds",   "EOR1  C, m.b",   "DEC   d",
"DEC   !a",       "MOV   Y, #i",    "POP   PSW",      "MOV   d, #i",
"BCC   r",        "TCALL 9",        "CLR1  d.4",      "BBC   d.4, r",
"ADC   A, d+X",   "ADC   A, !a+X",  "ADC   A, !a+Y",  "ADC   A, [d]+Y",
"ADC   d, #i",    "ADC   (X), (Y)", "SUBW  YA, d",    "DEC   d+X",
"DEC   A",        "MOV   X, SP",    "DIV   YA, X",    "XCN   A",
"EI",             "TCALL 10",       "SET1  d.5",      "BBS   d.5, r",
"SBC   A, d",     "SBC   A, !a",    "SBC   A, (X)",   "SBC   A, [d+X]",
"SBC   A, #i",    "SBC   dd, ds",   "MOV1  C, m.b",   "INC   d",
"INC   !a",       "CMP   Y, #i",    "POP   A",        "MOV   (X)+, A",
"BCS   r",        "TCALL 11",       "CLR1  d.5",      "BBC   d.5, r",
"SBC   A, d+X",   "SBC   A, !a+X",  "SBC   A, !a+Y",  "SBC   A, [d]+Y",
"SBC   d, #i",    "SBC   (X), (Y)", "MOVW  YA, d",    "INC   d+X",
"INC   A",        "MOV   SP, X",    "DAS   A",        "MOV   A, (X)+",
"DI",             "TCALL 12",       "SET1  d.6",      "BBS   d.6, r",
"MOV   d, A",     "MOV   !a, A",    "MOV   (X), A",   "MOV   [d+X], A",
"CMP   X, #i",    "MOV   !a, X",    "MOV1  m.b, C",   "MOV   d, Y",
"MOV   !a, Y",    "MOV   X, #i",    "POP   X",        "MUL   YA",
"BNE   r",        "TCALL 13",       "CLR1  d.6",      "BBC   d.6, r",
"MOV   d+X, A",   "MOV   !a+X, A",  "MOV   !a+Y, A",  "MOV   [d]+Y, A",
"MOV   d, X",     "MOV   d+Y, X",   "MOVW  d, YA",    "MOV   d+X, Y",
"DEC   Y",        "MOV   A, Y",     "CBNE  d+X, r",   "DAA   A",
"CLRV",           "TCALL 14",       "SET1  d.7",      "BBS   d.7, r",
"MOV   A, d",     "MOV   A, !a",    "MOV   A, (X)",   "MOV   A, [d+X]",
"MOV   A, #i",    "MOV   X, !a",    "NOT1  m.b",      "MOV   Y, d",
"MOV   Y, !a",    "NOTC",           "POP   Y",        "SLEEP",
"BEQ   r",        "TCALL 15",       "CLR1  d.7",      "BBC   d.7, r",
"MOV   A, d+X",   "MOV   A, !a+X",  "MOV   A, !a+Y",  "MOV   A, [d]+Y",
"MOV   X, d",     "MOV   X, d+Y",   "MOV   dd, ds",   "MOV   Y, d+X",
"INC   Y",        "MOV   Y, A",     "DBNZ  Y, r",     "STOP"
};

const char* spc_disasm_form( int opcode ) { return op_names [opcode]; }

int spc_disasm( unsigned addr, int opcode, int x, int y, char* out )
{
	// Interpret opcode format
	const char* in = op_names [opcode];
	while ( (*out = *in) != 0 )
	{
		switch ( *in++ )
		{
		case 'i': // #i
			out += sprintf( out, "$%02X", x );
			break;
		
		case 'u': // PCALL u
			out += sprintf( out, "$FF%02X", x );
			break;
		
		case 'd': { // d, dd, ds
			int n = y;
			if ( *in != 'd' )
			{
				n = x;
				x = y;
			}
			if ( *in == 'd' || *in == 's' )
				++in;
			
			out += sprintf( out, "$%02X", n );
			break;
		}
		
		case 'a': // !a
			out += sprintf( out, "$%04X", y * 0x100 + x );
			break;
		
		case 'm': // m.b
			out += sprintf( out, "$%04X", (y * 0x100 + x) & 0x1FFF );
			break;
		
		case 'b': // m.b
			out += sprintf( out, "%d", y >> 5 );
			break;
		
		case 'r': // branch r
			out += sprintf( out, "$%04X", (addr + op_lens [opcode] + (signed char) x) & 0xFFFF );
			break;
		
		default:
			++out;
		}
	}
	
	return op_lens [opcode];
}
