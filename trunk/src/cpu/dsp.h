// SNES SPC-700 DSP emulator

#ifndef SPC_DSP_H
#define SPC_DSP_H

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef __GNUC__
#define INLINE static
#else
#define INLINE static inline
#endif

//// Setup

// Initializes DSP and has it use the 64K RAM provided
void dsp_init( void* ram_64k );

// Mutes voices based on which bits are set
enum { dsp_voice_count = 8 };
void dsp_mute_voices( int mask );

// If true, prevents channels and global volumes from being phase-negated.
// This is something a few games do to achieve pseudo surround sound.
void dsp_disable_surround( bool disable );

// Sets gain, where 1.0 is normal. Output is clamped, so gains greater
// than 1.0 are fine.
void dsp_set_gain( double );


//// Emulation

// Resets DSP to power-on state. Does not affect anything set by above functions.
void dsp_reset( void );

// Reads/writes DSP registers
enum { dsp_register_count = 128 };
static int dsp_read( int addr );
void dsp_write( int addr, int data );

// Runs DSP and generates pair_count*2 16-bit samples into *out
typedef short dsp_sample_t;
void dsp_run( long pair_count, dsp_sample_t* out );


//// private

//extern uint8_t m_dsp_regs [dsp_register_count];
INLINE int dsp_read( int addr )
{
	assert( (unsigned) addr < dsp_register_count );
	//return m_dsp_regs [addr];
}

#endif
