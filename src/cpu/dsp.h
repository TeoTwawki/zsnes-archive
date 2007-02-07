/*
Copyright (C) 1997-2007 ZSNES Team ( zsKnight, _Demo_, pagefault, Nach )

http://www.zsnes.com
http://sourceforge.net/projects/zsnes
https://zsnes.bountysource.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// SNES SPC-700 DSP emulator

#ifndef SPC_DSP_H
#define SPC_DSP_H

#include <stdint.h>

// Initializes DSP and has it use the 64K RAM provided
void dsp_init( void* ram_64k );

// Mutes voices based on which bits are set
enum { dsp_voice_count = 8 };
void dsp_mute_voices( int mask );

// If true, prevents channels and global volumes from being phase-negated
void dsp_disable_surround( int disable );

// Sets gain, where 1.0 is normal. Output is clamped, so gains greater
// than 1.0 are fine.
void dsp_set_gain( double );


// Resets DSP to power-on state. Does not affect anything set by above functions.
void dsp_reset( void );

// Reads/writes DSP registers
enum { dsp_register_count = 128 };
int dsp_read( int addr );
void dsp_write( int addr, int data );

// Runs DSP and generates pair_count*2 16-bit samples into *out
typedef short dsp_sample_t;
void dsp_run( long pair_count, dsp_sample_t* out );


// State save/restore *example*; expand/rewrite as necessary to make it robust
typedef struct dsp_voice_state_t
{
	int16_t interp [4];
	int16_t envx;
	int16_t env_timer;
	int16_t fraction;
	uint16_t addr;
	uint8_t block_remain;
	uint8_t block_header;
	uint8_t env_mode;
	uint8_t key_on_delay;
} dsp_voice_state_t;

enum { dsp_fir_buf_half = 8 };

typedef struct dsp_state_t
{
	uint8_t regs [dsp_register_count];
	dsp_voice_state_t voice_state [dsp_voice_count];
	uint16_t echo_pos;
	int16_t  noise_count;
	int16_t  noise;
	int16_t  fir_buf [dsp_fir_buf_half] [2];
	uint8_t  keys_down;
} dsp_state_t;

// Saves/loads DSP state
void dsp_save_state( dsp_state_t* out );
void dsp_load_state( dsp_state_t const* in );

#endif
