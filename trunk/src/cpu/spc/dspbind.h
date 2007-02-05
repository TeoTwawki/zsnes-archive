#ifndef DSPBIND_H
#define DSPBIND_H

#ifdef __cplusplus
extern "C" {
#endif


// Mute voice n if bit n (1 << n) of DSP_mask is clear.
extern int DSP_mask;
void dsp_mute_voices();

// Clear state and silence everything.
void dsp_reset();

// Set gain, where 1.0 is normal. When greater than 1.0, output is clamped to
// the 16-bit sample range.
extern double DSP_gain;
void dsp_set_gain();

// If true, prevent channels and global volumes from being phase-negated
extern int DSP_disable;
void dsp_disable_surround();

// Read/write DSP register

extern int DSP_reg, DSP_val;
int dsp_read();
void dsp_write();

// Run DSP for 'count' samples. Write resulting samples to 'buf' if not NULL.
extern int DSP_count;
extern short *DSP_buf;
void dsp_run();

#ifdef __cplusplus
}
#endif

#endif // !DSPBIND_H
