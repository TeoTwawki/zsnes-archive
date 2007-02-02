
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

void dsp_dsiable_surround(int bDisable) {
    theDsp.disable_surround(bDisable);
}

int dsp_read(int reg) {
    return theDsp.read(reg);
}

void dsp_write(int reg, int val) {
    dsp_write(reg, val);
}

void dsp_run(long count, short *buf) {
    dsp_run(count, buf);
}

}
