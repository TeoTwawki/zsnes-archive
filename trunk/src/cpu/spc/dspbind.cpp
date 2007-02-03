
#include "dsp.h"

extern "C" {

#include "dspbind.h"

extern unsigned char SPCRAM[0x10000];

int DSP_mask;
double DSP_gain;
int DSP_disable;
int DSP_reg, DSP_val;
int DSP_count;
short *DSP_buf;

static Spc_Dsp theDsp(SPCRAM);

void dsp_mute_voices() {
    theDsp.mute_voices(DSP_mask);
}

void dsp_reset() {
    theDsp.reset();
}

void dsp_set_gain() {
    theDsp.set_gain(DSP_gain);
}

void dsp_disable_surround() {
    theDsp.disable_surround(DSP_disable);
}

int dsp_read() {
    return theDsp.read(DSP_reg);
}

void dsp_write() {
    theDsp.write(DSP_reg, DSP_val);
}

void dsp_run() {
    theDsp.run(DSP_count, DSP_buf);
}

}