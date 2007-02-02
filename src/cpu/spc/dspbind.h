#ifndef DSPBIND_H
#define DSPBIND_H

// Mute voice n if bit n (1 << n) of mask is clear.
void dsp_mute_voices(int mask);

// Clear state and silence everything.
void dsp_reset();

// Set gain, where 1.0 is normal. When greater than 1.0, output is clamped to
// the 16-bit sample range.
void dsp_set_gain(double gain);

// If true, prevent channels and global volumes from being phase-negated
void dsp_disable_surround(int bDisable);

// Read/write DSP register
int dsp_read(int reg);
void dsp_write(int reg, int val);

// Run DSP for 'count' samples. Write resulting samples to 'buf' if not NULL.
void dsp_run(long count, short *buf);

#endif // !DSPBIND_H
