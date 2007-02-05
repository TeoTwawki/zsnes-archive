
#include "dsp.h"
#include "../../gblhdr.h"
#include "dspbind.h"

extern "C" {
  void write_audio(short *sample_buffer, size_t sample_count);
  extern unsigned char SPCRAM[0x10000];
  extern unsigned char cycpbl;
  extern unsigned int spcCycle;
}

int DSP_mask;
double DSP_gain;
int DSP_disable;
int DSP_reg, DSP_val;
int DSP_count;
int DSP_midframe;
short *DSP_buf;
int lastCycles;
static Spc_Dsp theDsp(SPCRAM);

void dsp_mute_voices()
{
  theDsp.mute_voices(DSP_mask);
}

void dsp_reset()
{
  theDsp.reset();
}

void dsp_set_gain()
{
  theDsp.set_gain(DSP_gain);
}

void dsp_disable_surround()
{
  theDsp.disable_surround(DSP_disable);
}

int dsp_read()
{
  return theDsp.read(DSP_reg);
}

void dsp_write()
{
  theDsp.write(DSP_reg, DSP_val);
}

short tempbuf[154096] = {0};
int lastCycle = 0;

struct
{
  unsigned long long hi;
  unsigned long long lo;
  unsigned long long balance;
} static sample_control = { 995ULL*32000ULL, 59649ULL, 995ULL*32000ULL };

void dsp_run()
{
  static int mid_samples = 0;
  if (DSP_midframe)
  {
    int samples = (spcCycle-lastCycle)/32;
    if (samples > 0)
    {
      printf("outputting samples: %d\n", samples);
      theDsp.run(samples*2, tempbuf);
      write_audio(tempbuf, samples);
      lastCycle = spcCycle;
      mid_samples += samples;
    }
  }
  else
  {
    int samples = (unsigned int)((sample_control.balance/sample_control.lo) << 1);
    sample_control.balance %= sample_control.lo;
    sample_control.balance += sample_control.hi;

    samples -= mid_samples;
    if (samples > 0)
    {
      printf("outputting samples: %d\n", samples);
      theDsp.run(samples*2, tempbuf);
      write_audio(tempbuf, samples);
    }
    mid_samples = 0;
  }
}
