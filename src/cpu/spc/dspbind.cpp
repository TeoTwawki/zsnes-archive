
#include "dsp.h"

extern "C" {

#include "dspbind.h"

extern unsigned char SPCRAM[0x1000];

static Spc_Dsp theDsp(SPCRAM);

void dsp_mute_voices(int mask) {
    theDsp.mute_voices(mask);
}

void dsp_reset() {
    theDsp.reset();
}

void dsp_set_gain(double gain) {
    theDsp.set_gain(gain);
}



}
