/*
Copyright (C) 1997-2007 ZSNES Team ( zsKnight, _Demo_, pagefault, Nach )

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

#include "ntsc.h"

#ifndef __MSDOS__
	/* Source image */
	/* width = 288 pixels, height = 223 pixels or more */
	extern unsigned short* vidbuffer;

	static snes_ntsc_t ntsc_snes;
	static unsigned char ntsc_phase = 0;

	static void ntsc_blit( snes_ntsc_t const* ntsc, unsigned short const* input, long in_pitch,
			int burst_phase, int out_width, int out_height, void* rgb_out, long out_pitch );
#endif

void NTSCFilterInit()
{
	#ifndef __MSDOS__
	{
		/* Set GUI options */
		snes_ntsc_setup_t ntsc_setup = snes_ntsc_composite; /* start with preset */
		float to_float = 1.0f / 100.0f; /* convert to -1.0 to +1.0 range */

		/* artifacts */
		float enhance = to_float * NTSCWarp; /* new use of NTSCWarp control */
		ntsc_setup.resolution   = (enhance < 0.0f ? 0.0f : enhance);
		ntsc_setup.artifacts    = -enhance;
		ntsc_setup.fringing     = -enhance;

		/* standard controls */
		ntsc_setup.sharpness    = to_float * NTSCSharp;
		ntsc_setup.hue          = to_float * NTSCHue;
		ntsc_setup.saturation   = to_float * NTSCSat;
		ntsc_setup.contrast     = to_float * NTSCCont;
		ntsc_setup.brightness   = to_float * NTSCBright;

		/*ntsc_setup.hue_warping = to_float * NTSCWarp; // not supported anymore */
		ntsc_setup.merge_fields = NTSCBlend;

		snes_ntsc_init( &ntsc_snes, &ntsc_setup );
	}
	#endif
}

void NTSCFilterDraw( int out_width, int out_height, int out_pitch, unsigned char* rgb16_out )
{
	#ifndef __MSDOS__
	{
		ntsc_blit( &ntsc_snes, vidbuffer+16, 576, ntsc_phase,
				out_width, out_height, rgb16_out, out_pitch );

		/* Change phase on alternating frames unless blending is enabled */
		if ( !NTSCBlend )
			ntsc_phase ^= 1;
	}
	#endif
}

/* custom blitter that doubles image height and darkens every other row */
static void ntsc_blit( snes_ntsc_t const* ntsc, unsigned short const* input, long in_pitch,
		int burst_phase, int out_width, int out_height, void* rgb_out, long out_pitch )
{
	int const final_pixels = 5;
	int chunk_count = (out_width - final_pixels) / snes_ntsc_out_chunk;
	int in_height = out_height >> 1;
	for ( ; in_height; --in_height )
	{
		unsigned short const* line_in = input;
		SNES_NTSC_BEGIN_ROW( ntsc, burst_phase,
				snes_ntsc_black, snes_ntsc_black, *line_in );
		/* use of __restrict allows compiler to optimize memory accesses better */
		unsigned short* __restrict line_out  = (unsigned short*) rgb_out;
		unsigned short* __restrict line_out2 = (unsigned short*) ((char*) line_out + out_pitch);
		int n;
		line_in++;
		
	/* output second scanline darkened by 25% */
	#define PIXEL_OUT( x )\
	{\
		unsigned pixel;\
		SNES_NTSC_RGB_OUT( x, pixel, 16 );\
		line_out  [x] = pixel;\
		line_out2 [x] = pixel - (pixel >> 2 & 0x39E7);\
	}

		for ( n = chunk_count; n; --n )
		{
			/* order of input and output pixels must not be altered */
			SNES_NTSC_COLOR_IN( 0, line_in [0] );
			PIXEL_OUT( 0 );
			PIXEL_OUT( 1 );
			
			SNES_NTSC_COLOR_IN( 1, line_in [1] );
			PIXEL_OUT( 2 );
			PIXEL_OUT( 3 );
			
			SNES_NTSC_COLOR_IN( 2, line_in [2] );
			PIXEL_OUT( 4 );
			PIXEL_OUT( 5 );
			PIXEL_OUT( 6 );
			
			line_in   += 3;
			line_out  += 7;
			line_out2 += 7;
		}
		
		/* finish final pixels */
		SNES_NTSC_COLOR_IN( 0, snes_ntsc_black );
		PIXEL_OUT( 0 );
		PIXEL_OUT( 1 );
		
		SNES_NTSC_COLOR_IN( 1, snes_ntsc_black );
		PIXEL_OUT( 2 );
		PIXEL_OUT( 3 );
		
		SNES_NTSC_COLOR_IN( 2, snes_ntsc_black );
		PIXEL_OUT( 4 );
		/* last pixels is cut off a bit, so would be good to uncomment these two */
		/* (and change final_pixels to 7), though this would require a minimum of */
		/* 602 output pixels instead of the current 600 */
		/*PIXEL_OUT( 5 ); */
		/*PIXEL_OUT( 6 ); */
		
		burst_phase = (burst_phase + 1) % snes_ntsc_burst_count;
		input = (unsigned short const*) ((char const*) input + in_pitch);
		rgb_out = (char*) rgb_out + out_pitch * 2;
	}
}

/* included here to avoid modifying makefile */
#include "snes_ntsc/snes_ntsc.c"
