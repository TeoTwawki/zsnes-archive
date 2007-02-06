
#include "dsp.h"
#include "../../gblhdr.h"
#include "dspbind.h"

extern "C" {
#ifdef __LIBAO__
  void SoundWrite_ao();
#endif
  void write_audio(short *sample_buffer, size_t sample_count);
  extern unsigned char SPCRAM[0x10000];
  extern unsigned char cycpbl;
  extern unsigned int spcCycle;
}

int DSP_mask;
double DSP_gain;
int DSP_disable;
int DSP_reg, DSP_val;
int DSP_midframe;
static Spc_Dsp theDsp(SPCRAM);

short dsp_samples_buffer[1280];
int dsp_sample_count;
static short dsp_buffer[1280];
int lastCycle;

static int mid_samples;
static int cycles_remaining;
static int next_samples;

struct
{
  unsigned long long hi;
  unsigned long long lo;
  unsigned long long balance;
} static sample_control;

void dsp_init(unsigned char is_pal)
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
  memset(dsp_buffer, 0, sizeof(dsp_buffer));
  mid_samples = next_samples = dsp_sample_count = cycles_remaining = lastCycle = 0;
}

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

void dsp_run()
{
  if (DSP_midframe)
  {
    int samples;

    div_t d = div((cycles_remaining+spcCycle)-lastCycle, 64);
    cycles_remaining = d.rem;
    samples = d.quot;
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
      theDsp.run(samples, dsp_buffer);
      memcpy(dsp_samples_buffer+mid_samples*2, dsp_buffer, samples*sizeof(short)*2);
      mid_samples += samples;
    }
    lastCycle = spcCycle;
  }
  else
  {
    int samples = next_samples-mid_samples;
    if (samples > 0)
    {
      theDsp.run(samples, dsp_buffer);
      memcpy(dsp_samples_buffer+mid_samples*2, dsp_buffer, samples*sizeof(short)*2);
    }
    dsp_sample_count = next_samples*2;
    mid_samples = 0;
    next_samples = (unsigned int)(sample_control.balance/sample_control.lo);
    sample_control.balance %= sample_control.lo;
    sample_control.balance += sample_control.hi;

#ifdef __LIBAO__
    SoundWrite_ao();
#endif
    //write_audio(dsp_samples_buffer, dsp_sample_count);
  }
}
