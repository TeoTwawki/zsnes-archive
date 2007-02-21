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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dspwrap.h"
#include "dsp.h"
#include "../asm_call.h"

//C++ style code in C
#define true 1
#define false 0

void InitSPC()
{
  extern unsigned char SPCRAM[0x10000];
  extern bool romispal;

  asm_call(InitSPCRegs);
  dsp_init(SPCRAM);
  dsp_reset();
  InitDSPControl(romispal);
}

int DSP_reg, DSP_val;

void dsp_read_wrap()
{
  DSP_val = dsp_read(DSP_reg);
}

void dsp_write_wrap()
{
  dsp_write(DSP_reg, DSP_val);
}

#ifdef __LIBAO__
void SoundWrite_ao();
#endif
void write_audio(short *sample_buffer, size_t sample_count);
extern unsigned char cycpbl;
extern unsigned int spcCycle;

short dsp_samples_buffer[1280*5]; //Buffer 5 frames for even PAL
const size_t dsp_buffer_size = sizeof(dsp_samples_buffer)/sizeof(short);
size_t dsp_sample_count;
int lastCycle;

static int mid_samples;
static int cycles_remaining;
static int next_samples;

struct sample_control_t
{
  unsigned long long hi;
  unsigned long long lo;
  unsigned long long balance;
};
static struct sample_control_t sample_control;

void InitDSPControl(unsigned char is_pal)
{
  if (is_pal)
  {
    sample_control.hi = 1ULL*32000ULL;
    sample_control.lo = 50ULL;
  }
  else
  {
    sample_control.hi = 995ULL*32000ULL;
    sample_control.lo = 59649ULL;
  }
  sample_control.balance = sample_control.hi;
  memset(dsp_samples_buffer, 0, sizeof(dsp_samples_buffer));
  mid_samples = next_samples = dsp_sample_count = lastCycle = cycles_remaining = 0;
  lastCycle = spcCycle = 32;
}

void dsp_samples_push(short *buffer, size_t amount)
{
  //printf("Filled: %u; More: %u\n", dsp_sample_count, amount);
  if (amount+dsp_sample_count > dsp_buffer_size)
  {
    amount = dsp_buffer_size-dsp_sample_count;
  }
  if (amount)
  {
    memcpy(dsp_samples_buffer+dsp_sample_count, buffer, amount*sizeof(short));
    dsp_sample_count += amount;
  }
}

size_t dsp_samples_pull(short *buffer, size_t amount)
{
  size_t extra = 0;
  if (amount > dsp_sample_count)
  {
    extra = amount-dsp_sample_count;
    amount = dsp_sample_count;
  }
  if (amount)
  {
    memcpy(buffer, dsp_samples_buffer, amount*sizeof(short));
    dsp_sample_count -= amount;
    memmove(dsp_samples_buffer, dsp_samples_buffer+amount, dsp_sample_count*sizeof(short));
  }
  if (extra)
  {
    memset(buffer+amount, 0, extra*sizeof(short));
  }
  return(amount);
}

void dsp_run_wrap()
{
  short buffer[1280*2]; //Big enough for two PAL frames

  int i = cycles_remaining+spcCycle-lastCycle, samples = i >> 5;
  cycles_remaining = i & 31;

  if (samples > 0)
  {
    //printf("outputting samples: %d\n", stereo_samples);

    dsp_run(samples, buffer);
    //write_audio(buffer, samples*2);
    dsp_samples_push(buffer, samples*2);
  }
  lastCycle = spcCycle;
}
