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

#include "spc.h"
#include "dsp.h"
#include "../asm_call.h"

//C++ style code in C
#define bool unsigned char
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

int dsp_read_wrap()
{
  return dsp_read(DSP_reg);
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
const unsigned int dsp_buffer_size = sizeof(dsp_samples_buffer)/sizeof(short);
int dsp_sample_count;
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

void dsp_fill(unsigned int stereo_samples)
{
  static unsigned int fill_loc = 0;
  // printf("outputting samples: %d\n", stereo_samples);

  dsp_sample_count += stereo_samples*2;
  if (fill_loc+stereo_samples*2 >= dsp_buffer_size)
  {
    unsigned int current_samples = (dsp_buffer_size-fill_loc)/2;
    dsp_run(current_samples, dsp_samples_buffer+fill_loc);
    write_audio(dsp_samples_buffer+fill_loc, current_samples*2);
    stereo_samples -= current_samples;
    fill_loc = 0;
  }
  if (stereo_samples)
  {
    dsp_run(stereo_samples, dsp_samples_buffer+fill_loc);
    write_audio(dsp_samples_buffer+fill_loc, stereo_samples*2);
    fill_loc += stereo_samples*2;
  }

  if (dsp_sample_count >= 512) //Prevent slowing down from crazy little writing
  {
#ifdef __LIBAO__
    SoundWrite_ao();
#endif
    dsp_sample_count = 0;
  }
}

int DSP_midframe;

void dsp_run_wrap()
{
  //if (DSP_midframe)
  //{
    int i = cycles_remaining+spcCycle-lastCycle, samples = i >> 5;
    cycles_remaining = i & 31;

    if (samples > 0)
      dsp_fill(samples);
/*
    while (samples > next_samples)
    {
      samples -= next_samples;
    }
    if (samples+mid_samples >= next_samples)
    {
      samples = next_samples-mid_samples;
      mid_samples = next_samples-samples;
    }
    if (samples > 0)
    {
      //printf("outputting samples: %d\n", samples);
      dsp_fill(samples);
      mid_samples += samples;
    }
  }
  else
  {
    int samples = next_samples-mid_samples;
    if (samples > 0)
    {
      dsp_fill(samples);
    }
    mid_samples = 0;
    next_samples = ???
  }
*/
  lastCycle = spcCycle;

  // debug stuff
  if (!DSP_midframe) {
      static lastframe = 0;
      printf("frame cycles: %d\n", lastCycle-lastframe);
      lastframe = lastCycle;
  }
}
