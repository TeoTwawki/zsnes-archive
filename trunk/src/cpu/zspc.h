#ifndef ZSPC_H
#define ZSPC_H

#include "cpu/spc.h"

//// Setup

// Initializes module
void zspc_init( void );

// Sets destination for output samples to zspc_buf, which can hold zspc_buf_size samples
extern short *zspc_buf;
extern int zspc_buf_size;
void zspc_set_output( void );

// Number of samples written to output since it was last set
int zspc_sample_count( void );


//// Emulation

// Resets SPC
void zspc_reset( void );

extern spc_time_t zspc_time;
extern int zspc_port;
extern int zspc_data;

// Reads/writes zspc_data from zspc_port at zspc_time
int zspc_read_port( void );
void zspc_write_port( void );

// Runs SPC to zspc_time and starts a new time frame at 0
void zspc_end_frame( spc_time_t end_time );


#endif ZSPC_H
