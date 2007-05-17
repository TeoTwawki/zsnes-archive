/* http://www.slack.net/~ant/ */

#include "resamp.h"

#include <stdint.h>
#include <string.h>
#include <limits.h>

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

#if INT_MAX < 0x7FFFFFFF
	#error "Requires that int type have at least 32 bits"
#endif

typedef short sample_t;

/* the extra is for extra_samples (see below), a hard-coded constant so it
wouldn't have to be defined in the header file */
sample_t resampler_buf [RESAMPLER_BUF_SIZE + 8];
sample_t* resampler_write_pos;

/* Code deals with pairs of samples (stereo). Most references to sample really
mean a pair of samples. */
enum { stereo = 2 };

/* How many extra samples to keep around. Linear interpolation needs one extra. */
enum { extra_samples = 1 * stereo };

/* Number of bits used for linear interpolation. Can't be greater than 15 or
temporary products overflow a 32-bit integer. */
enum { interp_bits = 15 };

/* Number of fractional bits in s_pos. By making it interp_bits + 32, extraction
of the high interp_bits will be more efficient on 32-bit machines. */
enum { fixed_bits = interp_bits + 32 }; 
typedef uint64_t fixed_t;
static fixed_t const unit = (fixed_t) 1 << fixed_bits;

static fixed_t s_step;     /* number of input samples for every output sample */
static fixed_t s_pos;      /* fraction of input sample already used */
static int s_prev [stereo];/* previous samples for two-point low-pass filter */

void resampler_set_rate( int in, int out )
{
	fixed_t numer = in * unit;
	s_step = (numer + out / 2) / out; /* round to nearest */
	
	if ( !resampler_write_pos )
		resampler_clear();
}

void resampler_clear( void )
{
	/* For most input/output ratios, the step fraction will not be exact, so
	error will periodically occur. If s_step is exact or was rounded up, error
	will be one less output sample. If rounded down, error will be one extra
	output sample. Since error can be in either direction, start with position
	of half the step. */
	s_pos = s_step / 2;
	
	s_prev [0] = 0;
	s_prev [1] = 0;
	resampler_write_pos = resampler_buf + extra_samples;
	memset( resampler_buf, 0, extra_samples * sizeof resampler_buf [0] );
}

int resampler_read( sample_t* out_begin, int count )
{
	sample_t*       out     = out_begin;
	sample_t* const out_end = out + count;
	
	sample_t const* in     = resampler_buf;
	sample_t* const in_end = resampler_write_pos - extra_samples;

	if ( in < in_end )
	{
		fixed_t const step = s_step;
		if ( step != unit )
		{
			/* local copies help optimizer */
			fixed_t pos = s_pos;
			int prev0 = s_prev [0];
			int prev1 = s_prev [1];
			
			/* Step fractionally through input samples to generate output samples */
			do
			{
				/* Linear interpolation between current and next input sample, based on
				fractional portion of position. */
				int const factor = pos >> (fixed_bits - interp_bits);
				
				#define INTERP( i, out ) \
					out = (in [0 + i] * ((1 << interp_bits) - factor) + in [2 + i] * factor) >> interp_bits;\
				
				/* interpolate left and right */
				INTERP( 0, out [0] )
				INTERP( 1, out [1] )
				out += stereo;
				
				/* increment fractional position and separate whole part */
				pos += step;
				in += (int) (pos >> fixed_bits) * stereo;
				pos &= unit - 1;
			}
			while ( in < in_end && out < out_end );
			
			s_prev [1] = prev1;
			s_prev [0] = prev0;
			s_pos = pos;
		}
		else
		{
			/* no resampling */
			int n = in_end - in;
			if ( n > count )
				n = count;
			memcpy( out, in, n * sizeof *out );
			in  += n;
			out += n;
		}
	}
	
	/* move unused samples to beginning of input buffer */
	{
		int result = out - out_begin;
		int remain = resampler_write_pos - in;
		if ( remain < 0 )
			remain = 0; /* occurs when reducing sample rate */
		resampler_write_pos = &resampler_buf [remain];
		memmove( resampler_buf, in, remain * sizeof *in );
		return result;
	}
}
