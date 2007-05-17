// zspc 0.9.0. http://www.slack.net/~ant/

#include "zspc.h"

#include "resamp.h"
#include "spc_filt.h"
#include "disasm.h"
#include <stdio.h>
#include <ctype.h>

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

// speeds emulation up a bit
#define SPC_DISABLE_TEMPO 1

// Included here rather than individually to avoid having to
// change several makefiles when source files are added/removed
#include "snes_spc.cpp"
#include "snes_spm.cpp"
#include "snes_sps.cpp"

uint32_t zspc_time;

static SNES_SPC         spc;
static zspc_sample_t*   out_begin;
static zspc_sample_t*   out_pos;
static zspc_sample_t*   out_end;
static SPC_Filter       filter;

static void set_buf()
{
	spc.set_output( resampler_buffer(), resampler_max_write() );
}

static void clear_buf()
{
	zspc_time = 0;
	resampler_clear();
	filter.clear();
	set_buf();
}

static void flush_to_resampler()
{
	int count = spc.sample_count();
	if ( count > resampler_max_write() )
	{
		assert( false ); // fails if too much sound was buffered
		count = resampler_max_write();
	}
	filter.run( resampler_buffer(), count );
	resampler_write( count );
}

void zspc_enable_filter( int enable )
{
	filter.enable( enable );
}

void zspc_set_gain( int gain )
{
	filter.set_gain( gain * (SPC_Filter::gain_unit / zspc_gain_unit) );
}

void zspc_set_bass( int bass )
{
	filter.set_bass( bass );
}

zspc_err_t zspc_init()
{
	zspc_err_t err = spc.init();
	if ( err ) return err;
	
	zspc_set_rate( 32000 );
	
	static unsigned char const spc_rom [SNES_SPC::rom_size] =
	{
		0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0,
		0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
		0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4,
		0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
		0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB,
		0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
		0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD,
		0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF
	};
	spc.init_rom( spc_rom );
	zspc_reset();
	
	return 0;
}

zspc_err_t zspc_set_rate( int rate )
{
	resampler_set_rate( 32000, rate );
	return 0;
}

void zspc_set_output( zspc_sample_t* p, zspc_sample_count_t n )
{
	out_pos = p;
	if ( p )
	{
		out_begin = p;
		out_end   = p + n;
	}
}

zspc_sample_count_t zspc_sample_count()
{
	return out_pos ? out_pos - out_begin : 0;
}

zspc_err_t zspc_play( zspc_sample_count_t n, zspc_sample_t* out )
{
	zspc_sample_t* const end = out + n;
	while ( (out += resampler_read( out, end - out )) < end ) // read from resampler
	{
		// refill resampler
		set_buf();
		spc.end_frame( RESAMPLER_BUF_SIZE / 2 * (zspc_clocks_per_sample / 2) );
		flush_to_resampler();
	}
	return 0; // TODO: check for CPU error
}

void zspc_reset         ()                                  { spc.reset();      clear_buf(); }
void zspc_soft_reset    ()                                  { spc.soft_reset(); clear_buf(); }
void zspc_mute_voices   ( int mask )                        { spc.mute_voices( mask ); }

zspc_err_t zspc_load_spc( void const* p, long n )
{
	zspc_err_t err = spc.load_spc( p, n );
	clear_buf();
	return err;
}

void zspc_clear_echo    ()                                  { spc.clear_echo(); }

void zspc_copy_state    ( unsigned char** p, zspc_copy_func_t f ) { spc.copy_state( p, f ); }
void zspc_init_header   ( void* spc_out )                   { SNES_SPC::init_header( spc_out ); }
void zspc_save_spc      ( void* spc_out )                   { spc.save_spc( spc_out ); }
int  zspc_check_kon     ()                                  { return spc.check_kon(); }

int zspc_disasm( int addr, char* out )
{
	out += sprintf( out, "%04X ", addr );
	byte const* instr = &spc.ram() [addr];
	return spc_disasm( addr, instr [0], instr [1], instr [2], out );
}

const char* zspc_log_cpu()
{
	// Save registers and see if PC changes after running
	SNES_SPC::regs_t r = spc.regs();
	spc.run_until( zspc_time );
	if ( spc.regs().pc == r.pc )
		return 0;
	
	// Disassemble first instruction
	char opcode [zspc_disasm_max];
	zspc_disasm( r.pc, opcode );
	
	// Decode PSW
	char flags [9] = "nvdbhizc";
	for ( int i = 0; i < 8; i++ )
		if ( r.psw << i & 0x80 )
			flags [i] = toupper( flags [i] );
	
	// Return disassembly string
	static char str [64 + spc_disasm_max];
	sprintf( str, "%-24s; A=%02X X=%02X Y=%02X SP=%03X PSW=%02X %s",
			opcode, (int) r.a, (int) r.x, (int) r.y, (int) r.sp + 0x100, (int) r.psw, flags );
	return str;
}


//// Functions called from asm

uint32_t zspc_port;
uint32_t zspc_data;

void zspc_read_port () { zspc_data = spc.read_port( zspc_time, zspc_port ); }
void zspc_write_port() { spc.write_port( zspc_time, zspc_port, zspc_data );  }

void zspc_flush_samples()
{
	spc.end_frame( zspc_time );
	zspc_time = 0;
	if ( out_pos )
	{
		flush_to_resampler();
		out_pos += resampler_read( out_pos, out_end - out_pos );
		set_buf();
	}
}

#include "spc_filt.cpp"
#include "disasm.c"
#include "resamp.c"
