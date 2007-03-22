/*
Copyright (C) 1997-2007 ZSNES Team ( zsKnight, _Demo_, pagefault, Nach )
Copyright (C) 2002 Brad Martin (original implementation)
Copyright (C) 2006 Adam Gashlin (hcs) (conversion to C)
Copyright (C) 2004-2007 Shay Green

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

#include "dsp.h"

#include "spc_endian.h"

#include <stddef.h>
#include <string.h>
#include <limits.h>

// must be last
#include "spc_source.h"

/* Optimization notes
- There are several caches of values that are of type 'int' to allow faster
access. This helps on the x86, where it can often use a value directly
if it's a full 32 bits. In particular, voice_t::volume, voice_t::interp,
dsp_t::fir_buf and dsp_t::fir_coeff are where this is employed.

- All 16-bit little-endian accesses are aligned (assuming the 64K RAM you
pass to dsp_init() is aligned).
*/

// Voice registers
typedef struct raw_voice_t
{
	int8_t  vol [2];
	uint8_t pitch [2];
	uint8_t srcn;
	uint8_t adsr [2];   // envelope rates for attack, decay, and sustain
	uint8_t gain;       // envelope gain (if not using ADSR)
	int8_t  envx;       // current envelope level
	int8_t  outx;       // current sample
	int8_t  unused [6];
} raw_voice_t;

// Global registers
typedef struct globals_t
{
	int8_t  x1 [12];
	int8_t  mvoll;      // 0C   Main Volume Left
	int8_t  efb;        // 0D   Echo Feedback
	int8_t  x2 [14];
	int8_t  mvolr;      // 1C   Main Volume Right
	int8_t  x3 [15];
	int8_t  evoll;      // 2C   Echo Volume Left
	uint8_t pmon;       // 2D   Pitch Modulation on/off for each voice
	int8_t  x4 [14];
	int8_t  evolr;      // 3C   Echo Volume Right
	uint8_t non;        // 3D   Noise output on/off for each voice
	int8_t  x5 [14];
	uint8_t kon;        // 4C   Key On for each voice
	uint8_t eon;        // 4D   Echo on/off for each voice
	int8_t  x6 [14];
	uint8_t koff;       // 5C   key off for each voice (instantiates release mode)
	uint8_t dir;        // 5D   source directory (wave table offsets)
	int8_t  x7 [14];
	uint8_t flg;        // 6C   flags and noise freq
	uint8_t esa;        // 6D
	int8_t  x8 [14];
	uint8_t endx;       // 7C   end of loop flags
	uint8_t edl;        // 7D   echo delay
	char    x9 [2];
} globals_t;

union regs_t // overlay voices, globals, and array access
{
	globals_t   g;
	uint8_t     reg [dsp_register_count];
	raw_voice_t voices [dsp_voice_count];
	int16_t     align; // ensures that data is aligned for 16-bit accesses
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
	int volume [2];     // copy of volume from DSP registers, or 0 if muted
	int interp [4];     // most recently decoded samples
	int envx;
	int env_timer;
	int fraction;
	int addr;           // current position in BRR decoding
	int block_remain;   // number of nybbles remaining in current block
	int block_header;   // header byte from current block
	int env_mode;
	int key_on_delay;
} voice_t;

enum { gain_bits = 8 };

enum { dsp_fir_buf_half = 8 };

//// Variables

// All variables with m_ prefix are currently global, but could be put into a struct

// fir_buf [i + 8] == fir_buf [i], to avoid wrap checking in FIR code
static int      m_fir_buf [dsp_fir_buf_half * 2] [2];
static int      m_fir_pos; // (0 to 7)
static int      m_fir_coeff [dsp_voice_count]; // copy of echo FIR constants as int, for faster access

static int      m_new_kon;
static int      m_echo_pos;
static int      m_noise_count;
static int      m_noise;

static int      m_mute_mask;

static int      m_gain;
static int      m_surround_threshold;

static voice_t  m_voice_state [dsp_voice_count];
static uint8_t* m_ram;
uint8_t         m_dsp_regs [dsp_register_count];

#define RAM     m_ram
#define REGS    (m_dsp_regs)
#define GLOBALS (*(globals_t*) &m_dsp_regs)
#define VOICES  ((raw_voice_t*) &m_dsp_regs)
#define SD      ((src_dir const*) &RAM [GLOBALS.dir * 0x100])


/// Setup

void dsp_init( void* ram_64k )
{
	m_ram = (uint8_t*) ram_64k;
	dsp_set_gain( 1.0 );
	dsp_disable_surround( 0 );
	dsp_mute_voices( 0 );
	dsp_reset();

	// be sure compiler didn't pad structures overlaying dsp registers
	assert( offsetof (struct globals_t,x9 [2]) == dsp_register_count );
	assert( sizeof (raw_voice_t) * dsp_voice_count == dsp_register_count );
}

void dsp_mute_voices( int mask )
{
  int i;
  m_mute_mask = mask;

  // update cached volume values
  for ( i = dsp_voice_count * 0x10; (i -= 0x10) >= 0; )
    dsp_write( i, REGS [i] );
}

void dsp_disable_surround( bool disable )
{
	m_surround_threshold = disable ? 0 : -0x4000;
}

void dsp_set_gain( double v )
{
	m_gain = (int) (v * (1 << gain_bits));
}


//// Emulation

static void soft_reset()
{
  int old_koff;
  // TODO: probably inaccurate
  m_new_kon   = 0;
  GLOBALS.flg = 0xE0;
  old_koff = GLOBALS.koff;
  dsp_write( 0x5C, 0 ); // KOFF
  GLOBALS.koff = old_koff;
}

void dsp_reset()
{
	m_noise_count = 0;
	m_noise       = 1;
	m_echo_pos    = 0;
	m_fir_pos     = 0;

	memset( REGS,          0, sizeof REGS );
	memset( m_voice_state, 0, sizeof m_voice_state );
	memset( m_fir_buf,     0, sizeof m_fir_buf );
	memset( m_fir_coeff,   0, sizeof m_fir_coeff );
	soft_reset();
}

void dsp_write( int addr, int data )
{
  int high, low;
  voice_t* v;

  assert( (unsigned) addr < dsp_register_count );

  REGS [addr] = data;
  high = addr >> 4;
  low  = addr & 0x0F;
  if ( low < 2 ) // voice volumes
  {
	  int left  = *(int8_t const*) &REGS [addr & ~1];
	  int right = *(int8_t const*) &REGS [addr |  1];
    int enabled;

    // eliminate surround only if enabled and signs of volumes differ
    if ( left * right < m_surround_threshold )
    {
      if ( left < 0 )
	      left  = -left;
      else
	      right = -right;
    }

    // apply per-voice muting
    v = &m_voice_state [high];
    enabled = (m_mute_mask >> high & 1) - 1;
    v->volume [0] = left  & enabled;
    v->volume [1] = right & enabled;
  }
  else if ( low >= 0x0C ) // globals
  {
	  if ( low == 0x0F ) // FIR coefficients
	  {
		  m_fir_coeff [high] = (int8_t) data; // sign-extend
	  }
	  else if ( addr == 0x4C ) // KON
	  {
		  m_new_kon = data;
	  }
	  else if ( addr == 0x5C ) // KOFF
	  {
		  voice_t* v = m_voice_state;
		  do
		  {
			  if ( data & 1 )
			  {
				  v->env_mode = state_release;
				  v->key_on_delay = 0;
			  }
			  v++;
		  }
		  while ( (data >>= 1) != 0 );
		  assert( v <= &m_voice_state [dsp_voice_count] );
	  }
  }
}

// Interleved gauss table (to improve cache coherency)
static short const gauss [512] =
{ // verified: matches that of SNES
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

// each period divides exactly into 0x7800
#define ENV( period ) (0x7800 / period)
enum { env_rate_init = ENV( 1 ) };
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

// if ( n < -32768 ) out = -32768;
// if ( n >  32767 ) out =  32767;
#define CLAMP16( io )\
{\
	if ( (int16_t) io != io )\
		io = 0x7FFF ^ (io >> 31);\
}

void dsp_run( long count, dsp_sample_t* out_buf )
{
  int mvoll, mvolr, evoll, evolr, prev_outx, echol, echor, voicesl, voicesr, vbit;
  raw_voice_t* __restrict raw_voice;
  voice_t* __restrict voice;

	if ( GLOBALS.flg & 0x80 )
		soft_reset();

	{
		int kon = m_new_kon;
		GLOBALS.endx &= ~kon;
		kon &= ~GLOBALS.koff;
		m_new_kon = 0;
		if ( kon )
		{
			voice_t* v = m_voice_state;
			do
			{
				if ( kon & 1 )
					v->key_on_delay = 8;
				v++;
			}
			while ( (kon >>= 1) != 0 );
			assert( v <= &m_voice_state [dsp_voice_count] );
		}
	}

	// Global volumes
	mvoll = GLOBALS.mvoll;
	mvolr = GLOBALS.mvolr;
	if ( mvoll * mvolr < m_surround_threshold )
		mvolr = -mvolr; // eliminate surround
	mvoll = (mvoll * m_gain) >> gain_bits; // scale by emulator gain
	mvolr = (mvolr * m_gain) >> gain_bits;

	evoll = (GLOBALS.evoll * m_gain) >> gain_bits;
	evolr = (GLOBALS.evolr * m_gain) >> gain_bits;

	while ( --count >= 0 ) // generates one pair of output samples per iteration
	{
		// Run noise generator
		if ( GLOBALS.non ) // TODO: always running, even if no voices use it?
		{
			if ( (m_noise_count -= env_rates [GLOBALS.flg & 0x1F]) <= 0 )
			{
        int feedback;
				m_noise_count = env_rate_init;
				feedback = (m_noise << 13) ^ (m_noise << 14);
				m_noise = (feedback & 0x4000) ^ (m_noise >> 1);
			}
		}

		// Generate one sample for each voice
		prev_outx = echol = echor = voicesl = voicesr = 0;

		// restrict helps compiler optimize
		raw_voice = VOICES; // TODO: put raw_voice pointer in voice_t?
		voice = m_voice_state;
		vbit = 1;
		for ( ; vbit < 0x100; vbit <<= 1, ++voice, ++raw_voice )
		{
      int rate, offset, output, amp_0, amp_1;
      short const* fwd;
      short const* rev;

			// Key on events are delayed
			int key_on_delay = voice->key_on_delay;
			if ( --key_on_delay >= 0 ) // <1% of the time
			{
				voice->key_on_delay = key_on_delay;
				if ( key_on_delay == 0 )
				{
					voice->addr         = GET_LE16A( SD [raw_voice->srcn].start );
					voice->block_remain = 1;
					voice->envx         = 0;
					voice->block_header = 0; // "previous" BRR header
					voice->fraction     = 0x4000; // verified
					voice->interp [0]   = 0; // BRR decoder filter uses previous two samples
					voice->interp [1]   = 0;
					voice->env_timer    = env_rate_init; // TODO: inaccurate?
					voice->env_mode     = state_attack;
				}
			}

			// Envelope
			{
				int const env_range = 0x800;
				int envx = voice->envx;
				if ( voice->env_mode != state_release )
				{
					if ( raw_voice->adsr [0] & 0x80 )
					{
						if ( voice->env_mode == state_sustain )
						{
							if ( (voice->env_timer -= env_rates [raw_voice->adsr [1] & 0x1F]) <= 0 )
							{
								voice->env_timer = env_rate_init;
								envx--; // envx *= 255 / 256
								envx -= envx >> 8;
							}
						}
						else if ( voice->env_mode == state_decay )
						{
              int sustain_level;

							if ( (voice->env_timer -= env_rates [(raw_voice->adsr [0] >> 3 & 0x0E) + 0x10]) <= 0 )
							{
								voice->env_timer = env_rate_init;
								envx--; // envx *= 255 / 256
								envx -= envx >> 8;
							}

							sustain_level = raw_voice->adsr [1] >> 5;
							if ( envx <= (sustain_level + 1) * 0x100 )
								voice->env_mode = state_sustain;
						}
						else // state_attack
						{
							int t = raw_voice->adsr [0] & 0x0F;
							if ( (voice->env_timer -= env_rates [t * 2 + 1]) <= 0 )
							{
								voice->env_timer = env_rate_init;
								envx += (t < 15 ? env_range / 64 : env_range / 2);

								// TODO: Anomie claims envx >= env_range - 1
								if ( envx >= env_range )
								{
									envx = env_range - 1;
									voice->env_mode = state_decay;
								}
							}
						}
					}
					else if ( raw_voice->gain < 0x80 ) // direct gain
					{
						envx = raw_voice->gain << 4;
					}
					else if ( (voice->env_timer -= env_rates [raw_voice->gain & 0x1F]) <= 0 ) // envelope
					{
            int mode;

						voice->env_timer = env_rate_init;
						mode = raw_voice->gain >> 5;
						if ( mode < 5 ) // linear decrease
						{
							if ( (envx -= env_range / 64) < 0 )
								envx = 0;
						}
						else if ( mode == 5 ) // exponential decrease
						{
							envx--; // envx *= 255 / 256
							envx -= envx >> 8;
							assert( envx >= 0 );
						}
						else // increase
						{
							envx += (mode == 7 && envx >= env_range * 3 / 4 ? env_range / 256 : env_range / 64);
							if ( envx >= env_range )
								envx = env_range - 1;
						}
					}
				}
				else if ( (envx -= env_range / 256) <= 0 ) // state_release
				{
					voice->envx = 0;
					raw_voice->envx = 0;
					raw_voice->outx = 0;
					prev_outx = 0;
					continue; // TODO: should keep doing BRR decoding
				}
				voice->envx = envx;
				raw_voice->envx = envx >> 4;
			}

			// BRR decoding
			while ( voice->fraction >= 0x1000 )
			{
        int delta, shift;

				voice->fraction -= 0x1000;

				// Starting new block
				if ( !--voice->block_remain )
				{
					if ( voice->block_header & 1 )
					{
						GLOBALS.endx |= vbit;
						voice->addr = GET_LE16A( SD [raw_voice->srcn].loop );

						if ( !(voice->block_header & 2) ) // 1% of the time
						{
							// first block was end block; don't play anything (verified)
							// TODO: Anomie's doc says that SPC actually keeps decoding
							// sample, but you don't hear it because envx is set to 0.
				sample_ended:
							GLOBALS.endx |= vbit;

							// since voice->envx is 0, samples and position don't matter
							raw_voice->envx = 0;
							voice->envx = 0;
							voice->env_mode = state_release;
							break;
						}
					}

					voice->block_remain = 16; // nybbles
					voice->block_header = RAM [voice->addr];
					voice->addr = (voice->addr + 1) & 0xFFFF;
				}

				// if next block has end flag set, this block ends early (verified)
				// TODO: how early it ends depends on playback rate
				if ( voice->block_remain == 9 && (RAM [(voice->addr + 5) & 0xFFFF] & 3) == 1 &&
						(voice->block_header & 3) != 3 )
					goto sample_ended;

				// Get nybble and sign-extend
				delta = RAM [voice->addr];
				if ( voice->block_remain & 1 )
				{
					delta <<= 4; // use lower nybble
					voice->addr = (voice->addr + 1) & 0xFFFF;
				}
				delta = (int8_t) delta >> 4;

				// Scale delta based on range from header
				shift = voice->block_header >> 4;
				delta = (delta << shift) >> 1;
				if ( shift > 0x0C ) // handle invalid range
					delta = (delta >> 25) << 11; // delta = (delta < 0 ? ~0x7FF : 0)

				// Apply IIR filter
				if ( voice->block_header & 8 )
				{
					delta += voice->interp [0];
					delta -= voice->interp [1] >> 1;
					if ( !(voice->block_header & 4) )
					{
						delta += (-voice->interp [0] - (voice->interp [0] >> 1)) >> 5;
						delta += voice->interp [1] >> 5;
					}
					else
					{
						delta += (-voice->interp [0] * 13) >> 7;
						delta += (voice->interp [1] + (voice->interp [1] >> 1)) >> 4;
					}
				}
				else if ( voice->block_header & 4 )
				{
					delta += voice->interp [0] >> 1;
					delta += (-voice->interp [0]) >> 5;
				}

				// Shift samples over and condition new sample
				voice->interp [3] = voice->interp [2];
				voice->interp [2] = voice->interp [1];
				voice->interp [1] = voice->interp [0];
				CLAMP16( delta );
				voice->interp [0] = (int16_t) (delta * 2); // sign-extend
			}

			// Get rate (with possible modulation)
			rate = GET_LE16A( raw_voice->pitch ) & 0x3FFF; // verified
			if ( GLOBALS.pmon & vbit ) // verified
			{
				rate += ((prev_outx >> 5) * rate) >> 10; // verified
				if ( rate > 0x3FFF ) rate = 0x4000; // verified
			}

			// Gaussian interpolation using most recent 4 samples
			offset = voice->fraction >> 4; // verified
			voice->fraction += rate;

			// Gaussian interpolation using most recent 4 samples
			fwd = gauss       + offset * 2;
			rev = gauss + 510 - offset * 2; // mirror left half of gaussian
			output = 0;
			output += (fwd [0] * voice->interp [3]) >> 11; // verified: low bit is preserved
			output += (fwd [1] * voice->interp [2]) >> 11; // verified: low bit is preserved
			output += (rev [1] * voice->interp [1]) >> 11; // verified: low bit is preserved
			output = (int16_t) output; // verified
			output += (rev [0] * voice->interp [0]) >> 11; // verified: low bit is preserved
			CLAMP16( output ); // verified
			output &= ~1; // verified

			if ( GLOBALS.non & vbit )
				output = (int16_t) (m_noise * 2);

			output = (output * voice->envx) >> 11 & ~1; // verified: low bit cleared here
			prev_outx = output; // verified
			raw_voice->outx = (int8_t) (output >> 8); // TODO: verify

			amp_0 = (output * voice->volume [0]) >> 7;
			amp_1 = (output * voice->volume [1]) >> 7;

			//amp_0 &= ~1; // verified: low bit not cleared here
			//amp_1 &= ~1;

			voicesl += amp_0;
			voicesr += amp_1;

			CLAMP16( voicesl );
			CLAMP16( voicesr );

			if ( GLOBALS.eon & vbit )
			{
				echol += amp_0;
				echor += amp_1;

				CLAMP16( echol );
				CLAMP16( echor );

				//echol &= ~1; // verified: low bit not cleared here
				//echor &= ~1;
			}
		}
		// end of voice loop

		// Echo buffer position
    {
      int fbl, fbr;
      int (*fir_ptr) [2];

      uint8_t* const echo_ptr = &RAM [(GLOBALS.esa * 0x100 + m_echo_pos) & 0xFFFF];


		  m_echo_pos += 4;
		  if ( m_echo_pos >= (GLOBALS.edl & 15) * 0x800 )
			  m_echo_pos = 0;

		  // Keep most recent 8 echo samples
		  fir_ptr = &m_fir_buf [m_fir_pos];
		  m_fir_pos = (m_fir_pos + 1) & (dsp_fir_buf_half - 1);
		  fir_ptr [dsp_fir_buf_half] [0] = fir_ptr [0] [0] = GET_LE16SA( echo_ptr     ) >> 1;
		  fir_ptr [dsp_fir_buf_half] [1] = fir_ptr [0] [1] = GET_LE16SA( echo_ptr + 2 ) >> 1;

		  // Apply FIR
		  fbl = 0;
		  fbr = 0;

		  #define CALC_FIR( i )\
			  fbl += (fir_ptr [i + 1] [0] * m_fir_coeff [i]) >> 6;\
			  fbr += (fir_ptr [i + 1] [1] * m_fir_coeff [i]) >> 6;

		  CALC_FIR( 0 )
		  CALC_FIR( 1 )
		  CALC_FIR( 2 )
		  CALC_FIR( 3 )
		  CALC_FIR( 4 )
		  CALC_FIR( 5 )
		  CALC_FIR( 6 )

		  fbl = (int16_t) fbl;
		  fbr = (int16_t) fbr;

		  CALC_FIR( 7 )

		  #undef CALC_FIR

		  CLAMP16( fbl );
		  CLAMP16( fbr );

		  // Feedback into echo buffer
		  if ( !(GLOBALS.flg & 0x20) )
		  {
			  int efb = GLOBALS.efb;
			  int el = (((fbl & ~1) * efb) >> 7) + echol;
			  int er = (((fbr & ~1) * efb) >> 7) + echor;

			  CLAMP16( el );
			  CLAMP16( er );

			  el &= ~1;
			  er &= ~1;

			  SET_LE16A( echo_ptr    , el );
			  SET_LE16A( echo_ptr + 2, er );
		  }

		  // Generate output
		  if ( out_buf )
		  {
			  // low bits probably cleared at various points, but DAC noise makes it irrelevant

			  int outl = ((voicesl * mvoll) >> 7) + ((fbl * evoll) >> 7);
			  int outr = ((voicesr * mvolr) >> 7) + ((fbr * evolr) >> 7);

			  if ( GLOBALS.flg & 0x40 ) // global muting
			  {
				  outl = 0;
				  outr = 0;
			  }

			  CLAMP16( outl );
			  CLAMP16( outr );

			  out_buf [0] = outl;
			  out_buf [1] = outr;
			  out_buf += 2;
		  }
    }
	}
}
