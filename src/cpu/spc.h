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

#ifndef SPC_H
#define SPC_H

typedef int spc_time_t;
 
//// Setup
 
// Initializes module
void spc_init( void );
 
// Sets destination for output samples
void spc_set_output( short* out, int out_size );

// Number of samples written to output since it was last set
int spc_sample_count( void );
 
 
//// Emulation
 
// Resets SPC
void spc_reset( void );
 
// SPC-700 clock rate, in Hz
enum { spc_clock_rate = 1024000 };
 
// Number of I/O ports
enum { spc_port_count = 4 };
 
 // Reads/writes port at specified time
int spc_read_port( spc_time_t, int port );
void spc_write_port( spc_time_t, int port, int data );
 
// Runs SPC to end_time and starts a new time frame at 0
void spc_end_frame( spc_time_t end_time );


// TODO: save/load state
 
#endif
