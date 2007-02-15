#ifndef SPC_H
#define SPC_H

#include <stdint.h>

//// Setup

// Initializes module
void spc_init( void );

// Sets destination for output samples
typedef short spc_sample_t;
void spc_set_output( spc_sample_t* out, int out_size );

// Number of samples written to output since it was last set
int spc_sample_count( void );


//// Emulation

// Clock cycle count (at spc_clock_rate)
typedef int spc_time_t;

// Resets SPC
void spc_reset( void );

// SPC-700 clock rate, in Hz
enum { spc_clock_rate = 1024000 };

// Number of sample pairs per second
enum { spc_sample_rate = 32000 };

// Number of I/O ports
enum { spc_port_count = 4 };

// Reads/writes port at specified time
int spc_read_port( spc_time_t, int port );
void spc_write_port( spc_time_t, int port, int data );

// Runs SPC to end_time and starts a new time frame at 0
void spc_end_frame( spc_time_t end_time );

// TODO: save/load state


//// Private

typedef struct spc_cpu_regs_t
{
	long pc; // more than 16 bits to allow overflow detection
	uint8_t a;
	uint8_t x;
	uint8_t y;
	uint8_t status;
	uint8_t sp;
} spc_cpu_regs_t;

void spc_load_state_( spc_cpu_regs_t const* cpu_state,
		void const* ram_64k, void const* dsp_128regs );

#endif
