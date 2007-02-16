/*
Copyright (C) 1997-2007 ZSNES Team ( zsKnight, _Demo_, pagefault, Nach )
Copyright (C) 2002 Brad Martin (original implementation)
Copyright (C) 2006 Adam Gashlin (hcs) (conversion to C)
Copyright (C) 2004-2007 Shay Green

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

#include "spc.h"

#include "dsp.h"
#include "spc_cpu.h"

#include <string.h>

// must be last
#include "spc_source.h"

enum { dsp_rate_shift = 5 }; // dsp_clock_rate = spc_clock_rate >> dsp_rate_shift

enum { rom_size = 0x40 };
enum { rom_addr = 0xFFC0 };

enum { timer_count = 3 };
enum { timer_disabled_time = INT_MAX / 2 + 1 }; // always in the future

typedef struct Timer
{
	spc_time_t next_time; // time of next event
	int period;
	int count;
	int shift;
	int counter;
	bool enabled;
} Timer;

// All variables with m_ prefix are currently global, but could be put into a struct

static spc_sample_t* m_dsp_out_begin;
static spc_sample_t* m_dsp_out;
static spc_sample_t* m_dsp_out_end;
static spc_time_t    m_dsp_next_time; // time of next DSP sample

static Timer         m_timers [timer_count];
static uint8_t       m_out_ports [spc_port_count];
static uint8_t       m_extra_ram [rom_size];
static bool          m_rom_enabled;

static void enable_rom( bool enable )
{
	if ( m_rom_enabled != enable )
	{
		m_rom_enabled = enable;

		static uint8_t const boot_rom [rom_size] = {
			0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0,
			0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
			0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4,
			0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
			0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB,
			0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
			0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD,
			0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF
		};
		memcpy( RAM + rom_addr, (enable ? boot_rom : m_extra_ram), rom_size );
		// TODO: ROM can still get overwritten when DSP writes to echo buffer
	}
}


//// Setup

void spc_init( void )
{
	dsp_init( RAM );
	assert( (spc_clock_rate >> dsp_rate_shift) == spc_sample_rate );
}

void spc_set_output( spc_sample_t* out, int out_size )
{
	m_dsp_out_begin = out;
	m_dsp_out       = out;
	m_dsp_out_end   = out + out_size;
}

int spc_sample_count( void )
{
	return m_dsp_out - m_dsp_out_begin;
}

void spc_load_state_( spc_cpu_regs_t const* cpu_state,
		void const* ram_64k, void const* dsp_128regs )
{
        int i;
	SPC_TIME        = 0;
	m_dsp_next_time = 1 << dsp_rate_shift;

	// CPU
	spc_cpu_regs = *cpu_state;

	// Memory
	memcpy( RAM, ram_64k, sizeof RAM );
	memcpy( m_extra_ram, RAM + rom_addr, sizeof m_extra_ram );
	m_rom_enabled = !(RAM [0xF1] & 0x80); // force update
	enable_rom( !m_rom_enabled );

	// Put STOP instruction around memory to catch PC underflow/overflow
	memset( spc_ram_.padding1, 0xFF, sizeof spc_ram_.padding1 );
	memset( spc_ram_.padding2, 0xFF, sizeof spc_ram_.padding2 );

	// DSP
	dsp_reset();
	for ( i = 0; i < dsp_register_count; i++ )
		dsp_write( i, ((uint8_t const*) dsp_128regs) [i] );

	// Timers
	m_timers [0].shift = 4 + 3; // 8 kHz
	m_timers [1].shift = 4 + 3; // 8 kHz
	m_timers [2].shift = 4;     // 8 kHz
	for ( i = 0; i < timer_count; i++ )
	{
		Timer* t = &m_timers [i];

		int p = RAM [0xFA + i];
		t->period    = (p ? p : 0x100);
		t->counter   = RAM [0xFD + i] & 0x0F;
		t->count     = 0;
		t->enabled   = (RAM [0xF1] >> i) & 1;
		t->next_time = (t->enabled ? 0 : timer_disabled_time);
	}

	// Handle registers which already give 0 when read by setting RAM and not changing it.
	// Put STOP instruction in registers which can be read, to catch attempted execution.
	RAM [0xF0] = 0;
	RAM [0xF1] = 0;
	RAM [0xF3] = 0xFF;
	RAM [0xFA] = 0;
	RAM [0xFB] = 0;
	RAM [0xFC] = 0;
	RAM [0xFD] = 0xFF;
	RAM [0xFE] = 0xFF;
	RAM [0xFF] = 0xFF;

	m_out_ports [0] = 0;
	m_out_ports [1] = 0;
	m_out_ports [2] = 0;
	m_out_ports [3] = 0;
}

void spc_reset( void )
{
	// prepare state and restore it in place

	memset( RAM, 0, 0x10000 );
	RAM [0xF0] = 0x0A; // TEST
	RAM [0xF1] = 0xB0; // CONTROL
	RAM [0xFD] = 0x0F; // T0OUT
	RAM [0xFE] = 0x0F; // T1OUT
	RAM [0xFF] = 0x0F; // T2OUT

	uint8_t dsp [dsp_register_count];
	memset( dsp, 0, sizeof dsp );
	dsp [0x6C] = 0xE0; // FLG
	dsp [0x6D] = 0xFF; // ESA
	dsp [0x7D] = 0x01; // EDL

	static spc_cpu_regs_t cpu = { 0, 0, 0, 0, 0, 0 };
	cpu.pc = rom_addr;
	spc_load_state_( &cpu, RAM, dsp );
}


//// Timers

static void run_timer_( Timer* t, spc_time_t time )
{
	assert( t->enabled ); // disabled timer's next_time should always be in the future

	int elapsed = ((time - t->next_time) >> t->shift) + 1;
	t->next_time += elapsed << t->shift;

	elapsed += t->count;
	if ( elapsed >= t->period ) // avoid unnecessary division
	{
		int n = elapsed / t->period;
		elapsed -= n * t->period;
		t->counter = (t->counter + n) & 0x0F;
	}
	t->count = elapsed;
}

static inline void run_timer( Timer* t, spc_time_t time )
{
	if ( time >= t->next_time )
		run_timer_( t, time );
}


//// DSP

static void run_dsp_( spc_time_t time )
{
	int count = ((time - m_dsp_next_time) >> dsp_rate_shift) + 1;
	spc_sample_t* out = m_dsp_out;
	m_dsp_out = out + count * 2; // stereo

	assert( m_dsp_out <= m_dsp_out_end ); // fails if output buffer becomes full

	m_dsp_next_time += count << dsp_rate_shift;
	dsp_run( count, out );
}

static inline void run_dsp( spc_time_t time )
{
	if ( time >= m_dsp_next_time )
		run_dsp_( time );
}


//// CPU read/write

int spc_cpu_read( int addr )
{
	int result = RAM [addr];
	if ( addr >= 0xF0 )
	{
		result = RAM [addr & 0xFFFF];
		unsigned i = addr - 0xFD;
		if ( i < timer_count ) // counters
		{
			Timer* t = &m_timers [i];
			run_timer( t, SPC_TIME );
			result = t->counter;
			t->counter = 0;
		}
		else if ( addr == 0xF3 ) // dsp
		{
			run_dsp( SPC_TIME );
			result = dsp_read( RAM [0xF2] & 0x7F );
		}
	}

	return result;
}

void spc_cpu_write( int addr, int data )
{
        int i;
	// first page is very common
	if ( addr < 0xF0 )
	{
		RAM [addr] = (uint8_t) data;
	}
	else switch ( addr )
	{
		// RAM
		default:
			if ( addr < rom_addr )
			{
				RAM [addr] = (uint8_t) data;
			}
			else if ( addr >= 0x10000 )
			{
				RAM [addr - 0x10000] = (uint8_t) data;
			}
			else
			{
				m_extra_ram [addr - rom_addr] = (uint8_t) data;
				if ( !m_rom_enabled )
					RAM [addr] = (uint8_t) data;
			}
			break;

		case 0xF0: // Test register
			dprintf( "SPC wrote to test register\n" );
			break;

		// Config
		case 0xF1:
			// port clears
			if ( data & 0x10 )
			{
				RAM [0xF4] = 0;
				RAM [0xF5] = 0;
			}
			if ( data & 0x20 )
			{
				RAM [0xF6] = 0;
				RAM [0xF7] = 0;
			}

			// timers
			for ( i = 0; i < timer_count; i++ )
			{
				Timer* t = &m_timers [i];
				if ( !(data & (1 << i)) )
				{
					run_timer( t, SPC_TIME );
					t->enabled   = false;
					t->next_time = timer_disabled_time;
				}
				else if ( !t->enabled )
				{
					// just enabled
					t->enabled   = true;
					t->counter   = 0;
					t->count     = 0;
					t->next_time = SPC_TIME;
				}
			}

			enable_rom( (data & 0x80) != 0 );
			break;

		// DSP
		//case 0xF2: // mapped to RAM
		case 0xF3: {
			run_dsp( SPC_TIME );
			int reg = RAM [0xF2];
			if ( reg < dsp_register_count )
				dsp_write( reg, data & 0xFF );
			else
				dprintf( "SPC DSP write above 0x7F: $%02X\n", (int) reg );
			break;
		}

		// Ports
		case 0xF4:
		case 0xF5:
		case 0xF6:
		case 0xF7:
			m_out_ports [addr - 0xF4] = (uint8_t) data;
			break;

		//case 0xF8: // verified on SNES that these are read/write (RAM)
		//case 0xF9:

		// Timers
		case 0xFA:
		case 0xFB:
		case 0xFC: {
			int i = addr - 0xFA;
			Timer* t = &m_timers [i];
			data &= 0xFF;
			if ( (t->period & 0xFF) != data )
			{
				run_timer( t, SPC_TIME );
				t->period = data ? data : 0x100;
			}
			break;
		}

		// Counters (cleared on write)
		case 0xFD:
		case 0xFE:
		case 0xFF:
			dprintf( "SPC wrote to counter $%02X\n", (int) addr );
			m_timers [addr - 0xFD].counter = 0;
			break;
	}
}


//// Timing

static inline void run_until( spc_time_t end )
{
	assert( end >= SPC_TIME );
	spc_cpu_run( end );
	assert( SPC_TIME <= end );
}

int spc_read_port( spc_time_t t, int port )
{
	assert( (unsigned) port < spc_port_count );
	run_until( t );
	return m_out_ports [port];
}

void spc_write_port( spc_time_t t, int port, int data )
{
	assert( (unsigned) port < spc_port_count );
	run_until( t );
	RAM [0xF4 + port] = data;
}

void spc_end_frame( spc_time_t end )
{
        int i;
	if ( end > SPC_TIME )
		run_until( end );
	run_dsp( end );
	m_dsp_next_time -= end;
	assert( m_dsp_next_time >= 0 );

	SPC_TIME -= end;
	assert( SPC_TIME <= 0 );
	int const longest_instruction = 12; // DIV
	assert( SPC_TIME > -longest_instruction );

	for ( i = 0; i < timer_count; i++ )
	{
		Timer* t = &m_timers [i];
		if ( t->enabled )
		{
			t->next_time -= end;
			// If timer is being ignored, our timing variable will
			// underflow if timer weren't run regularly here
			run_timer( t, 0 );
		}
	}
}
