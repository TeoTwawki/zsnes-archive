// zspc 0.9.0. http://www.slack.net/~ant/

#include "zspc.h"

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

static SNES_SPC spc;

zspc_err_t zspc_init()
{
	zspc_err_t err = spc.init();
	if ( err ) return err;
	
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
	
	return 0;
}

void zspc_set_output( zspc_sample_t* p, zspc_sample_count_t n ) { spc.set_output( p, n ); }

zspc_sample_count_t zspc_sample_count()                         { return spc.sample_count(); }

zspc_err_t zspc_play( zspc_sample_count_t n, zspc_sample_t* p ) { return spc.play( n, p ); }

void zspc_reset         ( void )                            { spc.reset(); }
void zspc_soft_reset    ( void )                            { spc.soft_reset(); }
void zspc_mute_voices   ( int mask )                        { spc.mute_voices( mask ); }
zspc_err_t zspc_load_spc( void const* p, long n )           { return spc.load_spc( p, n ); }
void zspc_clear_echo    ( void )                            { spc.clear_echo(); }

void zspc_copy_state    ( unsigned char** p, zspc_copy_func_t f ) { spc.copy_state( p, f ); }
void zspc_init_header   ( void* spc_out )                   { SNES_SPC::init_header( spc_out ); }
void zspc_save_spc      ( void* spc_out )                   { spc.save_spc( spc_out ); }
int  zspc_check_kon     ( void )                            { return spc.check_kon(); }

//// Functions called from asm

uint32_t    zspc_time;
uint8_t     zspc_port;
uint8_t     zspc_data;

void zspc_read_port ()  { zspc_data = spc.read_port( zspc_time, zspc_port ); }
void zspc_write_port()  { spc.write_port( zspc_time, zspc_port, zspc_data ); }
void zspc_end_frame ()  { spc.end_frame( zspc_time ); }
