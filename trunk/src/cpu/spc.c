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

#include "dsp.h"
#include "../asm_call.h"

//C++ style code in C
#define bool unsigned char
#define true 1
#define false 0

void InitSPC(bool is_pal)
{
  extern unsigned char SPCRAM[0x10000];

  asm_call(InitSPCRegs);
  dsp_init(SPCRAM);
  dsp_reset();
  InitDSPControl(is_pal);
}
