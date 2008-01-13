/* Copyright (C) 2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include <assert.h>
#include <string.h>

struct instr_t { int n; const char* str; };

/* Instructions supported by assembler */
static struct instr_t const instrs [] =
{
	{0xD0,"BNE	@`"},
	{0x10,"BPL	@`"},
	{0x2F,"BRA	@`"},
	{0x78,"CMP	#$~~, $~~"},
	{0x7E,"CMP	Y, $~~"},
	{0x1D,"DEC	X"},
	{0xAB,"INC	$~~"},
	{0xFC,"INC	Y"},
	{0x1F,"JMP	[!$~~~~+X]"},
	{0x8F,"MOV	$~~, #$~~"},
	{0xC4,"MOV	$~~, A"},
	{0xCB,"MOV	$~~, Y"},
	{0xC6,"MOV	(X), A"},
	{0xE8,"MOV	A, #$~~"},
	{0xE4,"MOV	A, $~~"},
	{0xDD,"MOV	A, Y"},
	{0xBD,"MOV	SP, X"},
	{0xCD,"MOV	X, #$~~"},
	{0x5D,"MOV	X, A"},
	{0xEB,"MOV	Y, $~~"},
	{0xD7,"MOV	[$~~]+Y, A"},
	{0xDA,"MOVW	$~~, YA"},
	{0xBA,"MOVW	YA, $~~"},
	{0xC0,"$C0"},
	{0xFF,"$FF"},
	{-1,0}
};

/* Assembles SPC-700 assembly into machine code. Source points to array of string
pointers to source lines, terminated by a NULL string pointer. Very strict syntax,
no error reporting. */
static int assemble( const char* const* const source, unsigned char* const out )
{
	int labels [16] = { 0 };
	int pass;
	int addr;
	for ( pass = 2; pass--; )
	{
		int line;
		addr = 0;
		for ( line = 0; source [line]; line++ )
		{
			struct instr_t const* instr;
			const char* in = source [line];
			if ( *in++ == '@' )
			{
				labels [(*in - '0') & 0x0F] = addr;
				in += 3;
			}
			
			for ( instr = instrs; instr->str; instr++ )
			{
				int data = 1;
				int i = 0;
				for ( ; instr->str [i] && in [i]; i++ )
				{
					if ( instr->str [i] == '~' )
					{
						int n = in [i] - '0';
						if ( n > 9 )
							n -= 'A' - '9' - 1;
						data = data * 0x10 + n;
					}
					else if ( instr->str [i] == '`' )
					{
						data = ((labels [(in [i] - '0') & 0x0F] - 2 - addr) & 0xFF) | 0x100;
					}
					else if ( instr->str [i] != in [i] )
					{
						break;
					}
				}
				
				if ( instr->str [i] == in [i] )
				{
					out [addr++] = instr->n;
					while ( data > 1 )
					{
						out [addr++] = data & 0xFF;
						data >>= 8;
					}
					break;
				}
			}
		}
	}
	return addr;
}

/* Source code to bootloader for SPC-700 */
static const char* const bootrom_source [] =
{
	"	MOV	X, #$EF",	/* Initialize stack pointer */
	"	MOV	SP, X",
	"	MOV	A, #$00",	/* Clear page 0 */
	"@1:	MOV	(X), A",
	"	DEC	X",
	"	BNE	@1",
	"	MOV	$F4, #$AA",	/* Tell SNES we're ready */
	"	MOV	$F5, #$BB",
	"@2:	CMP	#$F4, $CC",	/* Wait for signal from SNES */
	"	BNE	@2",
	"	BRA	@6",		/* Get addr and command */
	"@3:	MOV	Y, $F4",	/* Wait for signal from SNES */
	"	BNE	@3",
	"@4:	CMP	Y, $F4",	/* Signal should be low byte of addr	 */
	"	BNE	@5",
	"	MOV	A, $F5",	/* Get byte to write */
	"	MOV	$F4, Y",	/* Acknowledge to SNES */
	"	MOV	[$00]+Y, A",	/* Write to destination */
	"	INC	Y",		/* Increment addr */
	"	BNE	@4",
	"	INC	$01",		/* Increment high byte of addr */
	"@5:	BPL	@4",		/* Keep waiting if <= low byte of addr */
	"	CMP	Y, $F4",	/* Stop if signal > low byte of addr */
	"	BPL	@4",
	"@6:	MOVW	YA, $F6",	/* Get addr */
	"	MOVW	$00, YA",
	"	MOVW	YA, $F4",	/* Get command */
	"	MOV	$F4, A",	/* Acknowledge to SNES */
	"	MOV	A, Y",
	"	MOV	X, A",
	"	BNE	@3",		/* non-zero = transfer */
	"	JMP	[!$0000+X]",	/* zero = execute */
	"	$C0",			/* reset vector */
	"	$FF",
	0
};

/* Assembles SPC-700 IPL bootrom and writes to out */
static void assemble_bootrom( unsigned char out [0x40] )
{
	int len = assemble( bootrom_source, out );
	assert( len == 0x40 );
}
