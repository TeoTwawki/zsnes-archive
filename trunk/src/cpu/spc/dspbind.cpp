
#include "dsp.h"
#include "../../gblhdr.h"
#include "dspbind.h"

extern "C" {
  void write_audio(short *sample_buffer, size_t sample_count);
  extern unsigned char SPCRAM[0x10000];
  extern unsigned char cycpbl;
  extern unsigned int spcCycle;
}

int samples_total;
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

short tempbuf[4096] = {0};
int lastCycle = 0;

void dsp_run()
{
  if (DSP_midframe)
  {
    theDsp.run(((spcCycle-lastCycle)/32), tempbuf);
    samples_total=((spcCycle-lastCycle)/32);
    lastCycle = spcCycle;
    //if (samples_total > 0)
    write_audio(tempbuf, samples_total);
    printf("outputting samples: %d\n", samples_total);
  }
  else
  {
    theDsp.run(DSP_count*2, tempbuf);
     samples_total = DSP_count*2;
    printf("outputting samples: %d\n", samples_total);
    write_audio(tempbuf, samples_total);
  }
}
