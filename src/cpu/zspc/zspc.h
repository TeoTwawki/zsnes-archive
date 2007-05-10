// SPC emulator interface customized for zsnes 

// zspc 0.9.0 
#ifndef ZSPC_H
#define ZSPC_H

#include <stddef.h>
#include "stdint.h"

#ifdef __cplusplus
	extern "C" {
#endif


// Error type returned from functions which can fail. NULL on success,
// otherwise pointer to error string.
typedef const char* zspc_err_t;

// 16-bit signed sample
typedef short zspc_sample_t;

// Sample count. Always a multiple of 2, since output is in stereo pairs.
typedef int zspc_sample_count_t;

// 1024000 SPC clocks per second, sample pair every 32 clocks, 32000 pairs per second
enum { zspc_clock_rate = 1024000 };
enum { zspc_clocks_per_sample = 32 };
enum { zspc_sample_rate = 32000 };

//// Setup

// Initializes SPC emulator
zspc_err_t zspc_init( void );

// Sets destination for output samples. If out is NULL, output samples are
// thrown away.
void zspc_set_output( zspc_sample_t* out, zspc_sample_count_t out_size );

// Number of samples written to output since last call to zspc_set_output()
zspc_sample_count_t zspc_sample_count( void );

// Resets SPC to power-on state. This resets your output buffer, so you must
// call zspc_set_output() after this.
void zspc_reset( void );

// Emulates pressing reset switch on SNES. This resets your output buffer, so
// you must call zspc_set_output() after this.
void zspc_soft_reset( void );

// Mutes voices corresponding to non-zero bits in mask. Reduces emulation accuracy.
enum { zspc_voice_count = 8 };
void zspc_mute_voices( int mask );


//// SPC music playback

// Loads SPC data into emulator
zspc_err_t zspc_load_spc( void const* zspc_in, long size );

// Clears echo region. Useful after loading an SPC as many have garbage in echo.
void zspc_clear_echo( void );

// Plays for count samples and writes samples to out. Discards samples if out
// is NULL.
zspc_err_t zspc_play( zspc_sample_count_t count, zspc_sample_t* out );


//// State save/load

// Saves/loads exact emulator state
enum { zspc_state_size = 67 * 1024L }; // maximum space needed when saving
typedef void (*zspc_copy_func_t)( unsigned char** io, void* state, size_t );
void zspc_copy_state( unsigned char** io, zspc_copy_func_t );

// Writes minimal SPC file header to spc_out
void zspc_init_header( void* spc_out );

// Saves emulator state as SPC file data. Writes zspc_file_size bytes to zspc_out.
// Does not set up SPC header; use zspc_init_header() for that.
enum { zspc_file_size = 0x10200 }; // spc_out must have this many bytes allocated
void zspc_save_spc( void* spc_out );

// Returns non-zero if new key-on events occurred since last check. Useful for
// trimming silence while saving an SPC.
int zspc_check_kon( void );


//// Functions called from asm

// These globals are used to pass values to/from functions below
extern uint32_t zspc_time;
extern uint8_t  zspc_port;
extern uint8_t  zspc_data;

// Reads/writes port at specified time
enum { zspc_port_count = 4 };
void/* zspc_data */ zspc_read_port (/* zspc_time, zspc_port */ void);
void/* void      */ zspc_write_port(/* zspc_time, zspc_port, zspc_data */ void);

// Runs to specified time and starts a new time frame at 0
void zspc_end_frame(/* zspc_time */ void);


#ifdef __cplusplus
	}
#endif

#endif
