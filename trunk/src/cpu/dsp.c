#include "dsp.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

/* Copyright (C) 2002 Brad Martin (original implementation) */
/* Copyright (C) 2006-2007 Adam Gashlin (hcs) (conversion to C) */
/* Copyright (C) 2004-2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

/* Optimization notes
- There are several caches of values that are of type 'int' to allow faster
access. This helps on the x86, where it can often use a value directly
if it's a full 32 bits. In particular, voice_t::volume, voice_t::interp,
dsp_t::fir_buf and dsp_t::fir_coeff are where this is employed.

- All 16-bit little-endian accesses are aligned (assuming the 64K RAM you
pass to dsp_init() is aligned).
*/

#if INT_MAX < 0x7FFFFFFF
	#error "Requires that int type have at least 32 bits"
#endif

// Little-endian 16-bit accessors
#define GET_LE16A( addr )       (*(uint16_t*) (addr))
#define GET_LE16SA( addr )      (*( int16_t*) (addr))
#define SET_LE16A( addr, data ) (void) (*(uint16_t*) (addr) = (data))

// Voice registers
typedef struct raw_voice_t
{
	int8_t  volume [2];
	uint8_t rate [2];
	uint8_t waveform;
	uint8_t adsr [2];   // envelope rates for attack, decay, and sustain
	uint8_t gain;       // envelope gain (if not using ADSR)
	int8_t  envx;       // current envelope level
	int8_t  outx;       // current sample
	int8_t  unused [6];
} raw_voice_t;

// Global registers
typedef struct globals_t
{
	int8_t  unused1 [12];
	int8_t  volume_0;       	// 0C   Main Volume Left (-.7)
	int8_t  echo_feedback;      // 0D   Echo Feedback (-.7)
	int8_t  unused2 [14];
	int8_t  volume_1;       	// 1C   Main Volume Right (-.7)
	int8_t  unused3 [15];
	int8_t  echo_volume_0;  	// 2C   Echo Volume Left (-.7)
	uint8_t pitch_mods;         // 2D   Pitch Modulation on/off for each voice
	int8_t  unused4 [14];
	int8_t  echo_volume_1;  	// 3C   Echo Volume Right (-.7)
	uint8_t noise_enables;      // 3D   Noise output on/off for each voice
	int8_t  unused5 [14];
	uint8_t key_ons;            // 4C   Key On for each voice
	uint8_t echo_ons;           // 4D   Echo on/off for each voice
	int8_t  unused6 [14];
	uint8_t key_offs;           // 5C   key off for each voice (instantiates release mode)
	uint8_t wave_page;          // 5D   source directory (wave table offsets)
	int8_t  unused7 [14];
	uint8_t flags;              // 6C   flags and noise freq
	uint8_t echo_page;          // 6D
	int8_t  unused8 [14];
	uint8_t wave_ended;         // 7C
	uint8_t echo_delay;         // 7D   ms >> 4
	char    unused9 [2];
} globals_t;

union regs_t // overlay voices, globals, and array access
{
	globals_t	g;
	uint8_t		reg [dsp_register_count];
	raw_voice_t	voice [dsp_voice_count];
	int16_t		align; // ensures that data is aligned for 16-bit accesses
};

// Instrument source directory entry
typedef struct src_dir
{
	char start [2];
	char loop  [2];
} src_dir;

enum state_t { // explicit values since they are used in save state
	state_attack  = 0,
	state_sustain = 1,
	state_decay   = 2,
	state_release = 3
};

// Internal voice state
typedef struct voice_t
{
	int volume [2];		// copy of volume from DSP registers, or 0 if muted
	int interp [4];		// most recently decoded samples
	int envx;
	int env_timer;
	int fraction;
	int addr;			// current position in BRR decoding
	int block_remain;	// number of nybbles remaining in current block
	int block_header;	// header byte from current block
	int env_mode;
	int key_on_delay;
	int enabled; // 0 if muted in emulator (must be last member; see dsp_reset())
} voice_t;

enum { gain_bits = 8 };

// Full DSP state
struct dsp_t
{
	union regs_t r;
	
	uint8_t* ram;
	
	unsigned echo_pos;
	int  noise_count;
	int16_t  noise; // must be 16 bits
	int  keys_down;
	
	// fir_buf [i + 8] == fir_buf [i], to avoid wrap checking in FIR code
	int fir_pos; // (0 to 7)
	int fir_buf [dsp_fir_buf_half * 2] [2];
	int fir_coeff [dsp_voice_count]; // copy of echo FIR constants as int, for faster access
	
	int gain;
	int surround_threshold;
	
	voice_t voice_state [dsp_voice_count];
};

// Use d-> to access state, currently global. Array so that you can easily change
// functions to take a 'dsp_t* d' without having to change a lot of code.
static struct dsp_t d [1];

#define RAM d->ram

#define SD ((src_dir const*) &RAM [d->r.g.wave_page * 0x100])

void dsp_init( void* ram )
{
	d->ram = (uint8_t*) ram;
	dsp_set_gain( 1.0 );
	dsp_disable_surround( 0 );
	dsp_mute_voices( 0 );
	dsp_reset();
	
	// be sure compiler didn't pad structures overlaying dsp registers
	assert( offsetof (struct globals_t,unused9 [2]) == dsp_register_count );
	assert( sizeof (d->r.voice) == dsp_register_count );
}

void dsp_mute_voices( int mask )
{
	int i;
	for ( i = dsp_voice_count; --i >= 0; )
		d->voice_state [i].enabled = -(~mask >> i & 1);
	
	// update cached volume values
	for ( i = dsp_voice_count * 0x10; (i -= 0x10) >= 0; )
		dsp_write( i, d->r.reg [i] );
}

void dsp_disable_surround( int disable )
{
	d->surround_threshold = disable ? 0 : -0x4000;
}

void dsp_set_gain( double v )
{
	d->gain = (int) (v * (1 << gain_bits));
}


static void soft_reset()
{
	// TODO: inaccurate? (Anomie's doc says this acts as if keys_off is 0xFF)
	d->keys_down   = 0;
	d->r.g.key_ons = 0;
}

void dsp_reset()
{
	d->noise_count = 0;
	d->noise       = 2;
	d->echo_pos    = 0;
	d->fir_pos     = 0;
	
	int i;
	for ( i = dsp_voice_count; --i >= 0; )
	{
		voice_t* v = &d->voice_state [i];
		memset( v, 0, offsetof (voice_t,enabled) );
		v->env_mode = state_release;
	}
	
	memset( &d->r, 0, sizeof d->r );
	memset( d->fir_buf, 0, sizeof d->fir_buf );
	memset( d->fir_coeff, 0, sizeof d->fir_coeff );
	soft_reset();
}

// TODO: make inline and put in dsp.h
int dsp_read( int addr )
{
	assert( (unsigned) addr < dsp_register_count );
	return d->r.reg [addr];
}
	
void dsp_write( int addr, int data )
{
	assert( (unsigned) addr < dsp_register_count );
	
	d->r.reg [addr] = data;
	int high = addr >> 4;
	int low  = addr & 0x0F;
	if ( low < 2 ) // voice volumes
	{
		int left  = *(int8_t const*) &d->r.reg [addr & ~1];
		int right = *(int8_t const*) &d->r.reg [addr |  1];
		
		// eliminate surround only if enabled and signs of volumes differ
		if ( left * right < d->surround_threshold )
		{
			if ( left < 0 )
				left  = -left;
			else
				right = -right;
		}
		
		// apply per-channel muting
		voice_t* v = &d->voice_state [high];
		int enabled = v->enabled;
		v->volume [0] = left  & enabled;
		v->volume [1] = right & enabled;
	}
	else if ( low == 0x0F ) // FIR coefficients
	{
		d->fir_coeff [7 - high] = (int8_t) data; // sign-extend
	}
}

// if ( n < -32768 ) out = -32768;
// if ( n >  32767 ) out =  32767;
#define CLAMP16( n, out )\
{\
	if ( (int16_t) n != n )\
		out = 0x7FFF ^ (n >> 31);\
}

void dsp_run( long count, dsp_sample_t* out_buf )
{
	if ( d->r.g.flags & 0x80 )
		soft_reset();
	
	// Key on/off
	{
		int key_ons  = d->r.g.key_ons;
		int key_offs = d->r.g.key_offs;
		d->r.g.wave_ended &= ~key_ons;	// keying on a voice resets that bit in ENDX
		d->r.g.key_ons = key_ons & key_offs; // key_off bits prevent key_on from being acknowledged
		
		// process key events outside loop, since they won't re-occur
		voice_t* voice = &d->voice_state [8];
		int vbit = 0x80;
		do
		{
			--voice;
			if ( key_offs & vbit )
			{
				voice->env_mode     = state_release;
				voice->key_on_delay = 0;
			}
			else if ( key_ons & vbit )
			{
				voice->key_on_delay = 8;
			}
		}
		while ( (vbit >>= 1) != 0 );
	}
	
	// Global volume
	int global_vol_0;
	int global_vol_1;
	{
		int vol_0 = d->r.g.volume_0;
		int vol_1 = d->r.g.volume_1;
		
		// eliminate surround
		if ( vol_0 * vol_1 < d->surround_threshold )
			vol_1 = -vol_1; // eliminate surround
		
		// scale by gain
		global_vol_0 = (vol_0 * d->gain) >> gain_bits;
		global_vol_1 = (vol_1 * d->gain) >> gain_bits;
	}
	
	// each period divides exactly into 0x7800
	#define ENV( period ) (0x7800 / period)
	int const env_rate_init = ENV( 1 );
	static int const env_rates [0x20] =
	{
		        0, ENV(2048), ENV(1536), 
		ENV(1280), ENV(1024), ENV( 768), 
		ENV( 640), ENV( 512), ENV( 384), 
		ENV( 320), ENV( 256), ENV( 192), 
		ENV( 160), ENV( 128), ENV(  96), 
		ENV(  80), ENV(  64), ENV(  48), 
		ENV(  40), ENV(  32), ENV(  24), 
		ENV(  20), ENV(  16), ENV(  12), 
		ENV(  10), ENV(   8), ENV(   6), 
		ENV(   5), ENV(   4), ENV(   3), 
		ENV(   2), ENV(   1)
	};
	#undef ENV
	
	do // one pair of output samples per iteration
	{
		// Noise
		if ( d->r.g.noise_enables )
		{
			if ( (d->noise_count -= env_rates [d->r.g.flags & 0x1F]) <= 0 )
			{
				d->noise_count = env_rate_init;
				int feedback = (d->noise << 13) ^ (d->noise << 14);
				d->noise = (feedback & 0x8000) ^ ((uint16_t) d->noise >> 1 & ~1);
			}
		}
		
		// Generate one sample for each voice
		int prev_outx = 0;
		int echo_0 = 0;
		int echo_1 = 0;
		int chans_0 = 0;
		int chans_1 = 0;
		raw_voice_t* raw_voice = d->r.voice; // TODO: put raw_voice pointer in voice_t?
		voice_t* voice = d->voice_state;
		int vbit = 1;
		for ( ; vbit < 0x100; vbit <<= 1, ++voice, ++raw_voice )
		{
			// Key on events are delayed
			int key_on_delay = voice->key_on_delay;
			if ( --key_on_delay >= 0 ) // <1% of the time
			{
				voice->key_on_delay = key_on_delay;
				if ( key_on_delay == 0 )
				{
					d->keys_down |= vbit;
					voice->addr         = GET_LE16A( SD [raw_voice->waveform].start );
					voice->block_remain = 1;
					voice->envx         = 0;
					voice->block_header = 0; // "previous" BRR header
					voice->fraction     = 0x3FFF; // decode three samples immediately
					voice->interp [0]   = 0; // BRR decoder filter uses previous two samples
					voice->interp [1]   = 0;
					voice->env_timer    = env_rate_init; // TODO: inaccurate?
					voice->env_mode     = state_attack;
				}
			}
			
			if ( !(d->keys_down & vbit) ) // Silent channel
			{
		silent_chan:
				raw_voice->envx = 0;
				raw_voice->outx = 0;
				prev_outx = 0;
				continue;
			}
			
			// Envelope
			{
				int const env_range = 0x800;
				int env_mode = voice->env_mode;
				int adsr0 = raw_voice->adsr [0];
				int env_timer;
				if ( env_mode != state_release ) // 99% of the time
				{
					env_timer = voice->env_timer;
					if ( adsr0 & 0x80 ) // 79% of the time
					{
						int adsr1 = raw_voice->adsr [1];
						if ( env_mode == state_sustain ) // 74% of the time
						{
							if ( (env_timer -= env_rates [adsr1 & 0x1F]) > 0 )
								goto write_env_timer;
							
							int envx = voice->envx;
							envx--; // envx *= 255 / 256
							envx -= envx >> 8;
							voice->envx = envx;
							raw_voice->envx = envx >> 4;
							goto init_env_timer;
						}
						else if ( env_mode > state_sustain ) // 25% state_decay
						{
							int envx = voice->envx;
							if ( (env_timer -= env_rates [(adsr0 >> 3 & 0x0E) + 0x10]) <= 0 )
							{
								envx--; // envx *= 255 / 256
								envx -= envx >> 8;
								voice->envx = envx;
								raw_voice->envx = envx >> 4;
								env_timer = env_rate_init;
							}
							
							int sustain_level = adsr1 >> 5;
							if ( envx <= (sustain_level + 1) * 0x100 )
								voice->env_mode = state_sustain;
							
							goto write_env_timer;
						}
						else // state_attack
						{
							int t = adsr0 & 0x0F;
							if ( (env_timer -= env_rates [t * 2 + 1]) > 0 )
								goto write_env_timer;
							
							int envx = voice->envx;
							
							int const step = env_range / 64;
							envx += step;
							if ( t == 15 )
								envx += env_range / 2 - step;
							
							// TODO: Anomie claims envx >= env_range - 1
							if ( envx >= env_range )
							{
								envx = env_range - 1;
								voice->env_mode = state_decay;
							}
							voice->envx = envx;
							raw_voice->envx = envx >> 4;
							goto init_env_timer;
						}
					}
					else // gain mode
					{
						int t = raw_voice->gain;
						if ( t < 0x80 ) // direct gain
						{
							raw_voice->envx = t;
							voice->envx = t << 4;
							goto env_end;
						}
						else // gain
						{
							if ( (env_timer -= env_rates [t & 0x1F]) > 0 )
								goto write_env_timer;
							
							int envx = voice->envx;
							int mode = t >> 5;
							if ( mode < 5 ) // linear decrease
							{
								if ( (envx -= env_range / 64) < 0 )
									envx = 0;
							}
							else if ( mode == 5 ) // exponential decrease
							{
								envx--; // envx *= 255 / 256
								envx -= envx >> 8;
								assert( envx >= 0 ); // shouldn't ever go negative
							}
							else // increase
							{
								int const step = env_range / 64;
								envx += step;
								if ( mode == 7 && envx >= env_range * 3 / 4 + step )
									envx += env_range / 256 - step;
								
								if ( envx >= env_range )
									envx = env_range - 1;
							}
							voice->envx = envx;
							raw_voice->envx = envx >> 4;
							goto init_env_timer;
						}
					}
				}
				else // state_release
				{
					int envx = voice->envx;
					if ( (envx -= env_range / 256) > 0 )
					{
						voice->envx = envx;
						raw_voice->envx = envx >> 8; // TODO: should this be 4?
						goto env_end;
					}
					else
					{
						d->keys_down ^= vbit; // bit was set, so this clears it
						voice->envx = 0;
						goto silent_chan;
					}
				}
			init_env_timer:
				env_timer = env_rate_init;
			write_env_timer:
				voice->env_timer = env_timer;
			env_end:;
			}
			
			// BRR decoding
			{
				int samples_needed = voice->fraction >> 12;
				if ( samples_needed )
				{
					int block_remain = voice->block_remain;
					int block_header = voice->block_header;
					uint8_t const* addr = &RAM [voice->addr];
					do
					{
						// Starting new block
						if ( !--block_remain )
						{
							if ( block_header & 1 )
							{
								d->r.g.wave_ended |= vbit;
							
								addr = &RAM [GET_LE16A( SD [raw_voice->waveform].loop )];
								if ( !(block_header & 2) ) // 1% of the time
								{
									// first block was end block; don't play anything (verified)
									// TODO: Anomie's doc says that SPC actually keeps decoding
									// sample, but you don't hear it because envx is set to 0.
						sample_ended:
									d->r.g.wave_ended |= vbit;
									d->keys_down ^= vbit; // bit was set, so this clears it
									
									// since voice->envx is 0, samples and position don't matter
									raw_voice->envx = 0;
									voice->envx = 0;
									break;
								}
							}
							
							if ( addr >= &RAM [0x10000 - 9] )
								goto sample_ended; // TODO: handle wrap-around samples better?
							
							voice->block_header = block_header = *addr++;
							block_remain = 16; // nybbles
						}
						
						// if next block has end flag set, this block ends early (verified)
						// TODO: how early it ends depends on playback rate
						if ( block_remain == 9 && (addr [5] & 3) == 1 &&
								(block_header & 3) != 3 )
							goto sample_ended;
						
						// Get nybble and sign-extend
						int delta = *addr;
						if ( block_remain & 1 )
						{
							delta <<= 4; // use lower nybble
							addr++;
						}
						delta = (int8_t) delta >> 4;
						
						// Scale delta based on range from header
						int shift = block_header >> 4;
						delta = (delta << shift) >> 1;
						if ( shift > 0x0C ) // handle invalid range
							delta = (delta >> 25) << 11; // delta = (delta < 0 ? ~0x7FF : 0)
						
						// Apply IIR filter
						int smp1 = voice->interp [0];
						int smp2 = voice->interp [1];
						if ( block_header & 8 )
						{
							delta += smp1;
							delta -= smp2 >> 1;
							if ( !(block_header & 4) )
							{
								delta += (-smp1 - (smp1 >> 1)) >> 5;
								delta += smp2 >> 5;
							}
							else
							{
								delta += (-smp1 * 13) >> 7;
								delta += (smp2 + (smp2 >> 1)) >> 4;
							}
						}
						else if ( block_header & 4 )
						{
							delta += smp1 >> 1;
							delta += (-smp1) >> 5;
						}
						
						// Shift samples over and condition new sample
						voice->interp [3] = voice->interp [2];
						voice->interp [2] = smp2;
						voice->interp [1] = smp1;
						CLAMP16( delta, delta );
						voice->interp [0] = (int16_t) (delta * 2); // sign-extend
					}
					while ( --samples_needed );
					voice->block_remain = block_remain;
					voice->addr         = addr - RAM;
				}
			}
			
			// Interleved gauss table (to improve cache coherency).
			// gauss [i * 2 + j] = normal_gauss [(1 - j) * 256 + i]
			static short const gauss [512] =
			{
			370,1305, 366,1305, 362,1304, 358,1304, 354,1304, 351,1304, 347,1304, 343,1303,
			339,1303, 336,1303, 332,1302, 328,1302, 325,1301, 321,1300, 318,1300, 314,1299,
			311,1298, 307,1297, 304,1297, 300,1296, 297,1295, 293,1294, 290,1293, 286,1292,
			283,1291, 280,1290, 276,1288, 273,1287, 270,1286, 267,1284, 263,1283, 260,1282,
			257,1280, 254,1279, 251,1277, 248,1275, 245,1274, 242,1272, 239,1270, 236,1269,
			233,1267, 230,1265, 227,1263, 224,1261, 221,1259, 218,1257, 215,1255, 212,1253,
			210,1251, 207,1248, 204,1246, 201,1244, 199,1241, 196,1239, 193,1237, 191,1234,
			188,1232, 186,1229, 183,1227, 180,1224, 178,1221, 175,1219, 173,1216, 171,1213,
			168,1210, 166,1207, 163,1205, 161,1202, 159,1199, 156,1196, 154,1193, 152,1190,
			150,1186, 147,1183, 145,1180, 143,1177, 141,1174, 139,1170, 137,1167, 134,1164,
			132,1160, 130,1157, 128,1153, 126,1150, 124,1146, 122,1143, 120,1139, 118,1136,
			117,1132, 115,1128, 113,1125, 111,1121, 109,1117, 107,1113, 106,1109, 104,1106,
			102,1102, 100,1098,  99,1094,  97,1090,  95,1086,  94,1082,  92,1078,  90,1074,
			 89,1070,  87,1066,  86,1061,  84,1057,  83,1053,  81,1049,  80,1045,  78,1040,
			 77,1036,  76,1032,  74,1027,  73,1023,  71,1019,  70,1014,  69,1010,  67,1005,
			 66,1001,  65, 997,  64, 992,  62, 988,  61, 983,  60, 978,  59, 974,  58, 969,
			 56, 965,  55, 960,  54, 955,  53, 951,  52, 946,  51, 941,  50, 937,  49, 932,
			 48, 927,  47, 923,  46, 918,  45, 913,  44, 908,  43, 904,  42, 899,  41, 894,
			 40, 889,  39, 884,  38, 880,  37, 875,  36, 870,  36, 865,  35, 860,  34, 855,
			 33, 851,  32, 846,  32, 841,  31, 836,  30, 831,  29, 826,  29, 821,  28, 816,
			 27, 811,  27, 806,  26, 802,  25, 797,  24, 792,  24, 787,  23, 782,  23, 777,
			 22, 772,  21, 767,  21, 762,  20, 757,  20, 752,  19, 747,  19, 742,  18, 737,
			 17, 732,  17, 728,  16, 723,  16, 718,  15, 713,  15, 708,  15, 703,  14, 698,
			 14, 693,  13, 688,  13, 683,  12, 678,  12, 674,  11, 669,  11, 664,  11, 659,
			 10, 654,  10, 649,  10, 644,   9, 640,   9, 635,   9, 630,   8, 625,   8, 620,
			  8, 615,   7, 611,   7, 606,   7, 601,   6, 596,   6, 592,   6, 587,   6, 582,
			  5, 577,   5, 573,   5, 568,   5, 563,   4, 559,   4, 554,   4, 550,   4, 545,
			  4, 540,   3, 536,   3, 531,   3, 527,   3, 522,   3, 517,   2, 513,   2, 508,
			  2, 504,   2, 499,   2, 495,   2, 491,   2, 486,   1, 482,   1, 477,   1, 473,
			  1, 469,   1, 464,   1, 460,   1, 456,   1, 451,   1, 447,   1, 443,   1, 439,
			  0, 434,   0, 430,   0, 426,   0, 422,   0, 418,   0, 414,   0, 410,   0, 405,
			  0, 401,   0, 397,   0, 393,   0, 389,   0, 385,   0, 381,   0, 378,   0, 374,
			};
			
			// Get rate (with possible modulation)
			int rate = GET_LE16A( raw_voice->rate ) & 0x3FFF;
			if ( d->r.g.pitch_mods & vbit )
				rate = (rate * (prev_outx + 32768)) >> 15;

			// Gaussian interpolation using most recent 4 samples
			unsigned fraction = voice->fraction & 0xFFF;
			int offset = fraction >> 4;
			voice->fraction = fraction + rate;
			
			// Only left half of gaussian kernel is in table, so we must mirror for right half
			short const* fwd = gauss       + offset * 2;
			short const* rev = gauss + 510 - offset * 2;
			
			int output = d->noise;
			if ( !(d->r.g.noise_enables & vbit) )
			{
				// gaussian interpolation
				output = (fwd [0] * voice->interp [3]) & ~0xFFF;
				output = (output + fwd [1] * voice->interp [2]) & ~0xFFF;
				output = (output + rev [1] * voice->interp [1]) >> 12;
				output = (int16_t) (output * 2);
				output += ((rev [0] * voice->interp [0]) >> 12) * 2;
				CLAMP16( output, output );
			}
			output = (output * voice->envx) >> 11 & ~1;
			
			int amp_0 = output * voice->volume [0] >> 7;
			int amp_1 = output * voice->volume [1] >> 7;
			prev_outx = output;
			raw_voice->outx = (int8_t) (output >> 8);
			
			chans_0 += amp_0;
			chans_1 += amp_1;
			if ( d->r.g.echo_ons & vbit )
			{
				echo_0 += amp_0;
				echo_1 += amp_1;
			}
		}
		// end of voice loop
		
		// Read feedback from echo buffer
		int echo_pos = d->echo_pos;
		uint8_t* const echo_ptr = &RAM [(d->r.g.echo_page * 0x100 + echo_pos) & 0xFFFF];
		echo_pos += 4;
		if ( echo_pos >= (d->r.g.echo_delay & 15) * 0x800 )
			echo_pos = 0;
		d->echo_pos = echo_pos;
		int fb_0 = GET_LE16SA( echo_ptr     );
		int fb_1 = GET_LE16SA( echo_ptr + 2 );
		
		// Keep last 8 samples
		int (*fir_ptr) [2] = &d->fir_buf [d->fir_pos];
		d->fir_pos = (d->fir_pos + 1) & (dsp_fir_buf_half - 1);
		fir_ptr [               0] [0] = fb_0;
		fir_ptr [               0] [1] = fb_1;
		fir_ptr [dsp_fir_buf_half] [0] = fb_0; // duplicate at +8 eliminates wrap checking below
		fir_ptr [dsp_fir_buf_half] [1] = fb_1;
		
		// Apply FIR
		fb_0 *= d->fir_coeff [0];
		fb_1 *= d->fir_coeff [0];

		#define DO_PT( i )\
			fb_0 += fir_ptr [i] [0] * d->fir_coeff [i];\
			fb_1 += fir_ptr [i] [1] * d->fir_coeff [i];
		
		DO_PT( 1 )
		DO_PT( 2 )
		DO_PT( 3 )
		DO_PT( 4 )
		DO_PT( 5 )
		DO_PT( 6 )
		DO_PT( 7 )
		#undef DO_PT
		
		// Generate output
		int out_0 = (chans_0 * global_vol_0 >> 7) + (fb_0 * d->r.g.echo_volume_0 >> 14);
		int out_1 = (chans_1 * global_vol_1 >> 7) + (fb_1 * d->r.g.echo_volume_1 >> 14);
		if ( d->r.g.flags & 0x40 )
		{
			out_0 = 0;
			out_1 = 0;
		}
		
		if ( out_buf )
		{
			CLAMP16( out_0, out_0 );
			out_buf [0] = out_0;
			CLAMP16( out_1, out_1 );
			out_buf [1] = out_1;
			out_buf += 2;
		}
		
		// Feedback into echo buffer
		int e0 = (fb_0 * d->r.g.echo_feedback >> 14) + echo_0;
		int e1 = (fb_1 * d->r.g.echo_feedback >> 14) + echo_1;
		if ( !(d->r.g.flags & 0x20) )
		{
			CLAMP16( e0, e0 );
			SET_LE16A( echo_ptr    , e0 );
			CLAMP16( e1, e1 );
			SET_LE16A( echo_ptr + 2, e1 );
		}
	}
	while ( --count );
}

/* Objectives of save state:
- Keep a stable representation even when DSP implementation changes. This allows
easy changes to DSP implementation, like when optimizing it.

- Don't save any unnecessary state (cached values, etc.), as this would expose
implementation details. These can be recalculated after restoring.
*/

// Copy field of same name from in to out. Macro eliminates common error of
// copying to the wrong field. Also, this ensures that the copied value
// matches the original, in case there is an out-of-bound value.
#define COPY( in, out, name )\
	(out->name = in->name, assert( out->name == in->name ))

void dsp_save_state( dsp_state_t* out )
{
	COPY( d, out, echo_pos );
	COPY( d, out, keys_down );
	COPY( d, out, noise_count );
	COPY( d, out, noise );
	
	int i;
	for ( i = dsp_voice_count; --i >= 0; )
	{
		voice_t const* vin = &d->voice_state [i];
		dsp_voice_state_t* vout = &out->voice_state [i];
		
		COPY( vin, vout, interp [0] );
		COPY( vin, vout, interp [1] );
		COPY( vin, vout, interp [2] );
		COPY( vin, vout, interp [3] );
		COPY( vin, vout, fraction );
		COPY( vin, vout, envx );
		COPY( vin, vout, env_timer );
		COPY( vin, vout, block_remain );
		COPY( vin, vout, block_header );
		COPY( vin, vout, key_on_delay );
		COPY( vin, vout, env_mode );
		COPY( vin, vout, addr );
	}
	
	for ( i = dsp_fir_buf_half; --i >= 0; )
	{
		out->fir_buf [i] [0] = d->fir_buf [d->fir_pos + i] [0];
		out->fir_buf [i] [1] = d->fir_buf [d->fir_pos + i] [1];
	}
	memcpy( out->regs, d->r.reg, sizeof out->regs );
}

void dsp_load_state( dsp_state_t const* in )
{
	dsp_reset();
	
	COPY( in, d, echo_pos );
	COPY( in, d, keys_down );
	COPY( in, d, noise_count );
	COPY( in, d, noise );
	
	int i;
	for ( i = dsp_voice_count; --i >= 0; )
	{
		dsp_voice_state_t const* vin = &in->voice_state [i];
		voice_t* vout = &d->voice_state [i];
		
		COPY( vin, vout, interp [0] );
		COPY( vin, vout, interp [1] );
		COPY( vin, vout, interp [2] );
		COPY( vin, vout, interp [3] );
		COPY( vin, vout, fraction );
		COPY( vin, vout, envx );
		COPY( vin, vout, env_timer );
		COPY( vin, vout, block_remain );
		COPY( vin, vout, block_header );
		COPY( vin, vout, key_on_delay );
		COPY( vin, vout, env_mode );
		COPY( vin, vout, addr );
	}
	
	for ( i = dsp_fir_buf_half; --i >= 0; )
	{
		d->fir_buf [i] [0] = d->fir_buf [i + dsp_fir_buf_half] [0] = in->fir_buf [i] [0];
		d->fir_buf [i] [1] = d->fir_buf [i + dsp_fir_buf_half] [1] = in->fir_buf [i] [1];
	}
	
	// manually writing registers will properly restore cached state
	for ( i = dsp_register_count; --i >= 0; )
		dsp_write( i, in->regs [i] );
}

#undef COPY

//ZSNES Extras Begin

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
extern unsigned char SPCRAM[0x10000];
extern unsigned char cycpbl;
extern unsigned int spcCycle;

int DSP_reg, DSP_val;
int DSP_midframe;

short dsp_samples_buffer[1280*3]; //Buffer 3 frames for even PAL
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

void dsp_init_wrap(unsigned char is_pal)
{
  dsp_init(SPCRAM);

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
  mid_samples = next_samples = dsp_sample_count = cycles_remaining = lastCycle = 0;
}

void dsp_fill(unsigned int stereo_samples)
{
  static unsigned int fill_loc = 0;
  dsp_sample_count = stereo_samples*2;
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

#ifdef __LIBAO__
  SoundWrite_ao();
#endif
}

void dsp_run_wrap()
{
  if (DSP_midframe)
  {
    int i = cycles_remaining+spcCycle-lastCycle, samples = i >> 6;

    cycles_remaining = i & 63;

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
    next_samples = (unsigned int)(sample_control.balance/sample_control.lo);
    sample_control.balance %= sample_control.lo;
    sample_control.balance += sample_control.hi;
  }
  lastCycle = spcCycle;
}
