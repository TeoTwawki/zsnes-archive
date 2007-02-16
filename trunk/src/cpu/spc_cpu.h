// SNES SPC-700 CPU emulator

#ifndef SPC_CPU_H
#define SPC_CPU_H

#include "spc.h"

// Run to end_time without going over
void spc_cpu_run( spc_time_t end_time );

// Called by CPU. Must be defined by client. Client must mask 'data' with 0xFF,
// as the high bits may contain garbage.
int spc_cpu_read( int addr );
void spc_cpu_write( int addr, int data );

#define RAM         spc_ram_.ram
#define SPC_TIME    spc_time_


// private

struct spc_ram_t
{
	// padding to catch address outside 64K space
	union {
		uint8_t padding1 [0x100];
		uint16_t align; // makes compiler align data for 16-bit access
	} padding1 [1];
	uint8_t ram      [0x10000];
	uint8_t padding2 [0x100];
};

extern spc_cpu_regs_t spc_cpu_regs;
extern struct spc_ram_t spc_ram_;
extern spc_time_t spc_time_;

#endif
