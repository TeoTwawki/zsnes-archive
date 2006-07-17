/*
Copyright (C) 1997-2006 ZSNES Team ( zsKnight, _Demo_, pagefault, Nach )

http://www.zsnes.com
http://sourceforge.net/projects/zsnes

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



#ifdef __UNIXSDL__
#include "gblhdr.h"
#define DIR_SLASH "/"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#define DIR_SLASH "\\"
#endif
#include "../zpath.h"
#include "../md.h"
#include "../cfg.h"
#include "../asm_call.h"
#include "../zloader.h"

#define BIT(X) (1 << (X))

#ifdef __WIN32__
#include <io.h>
#define strcasecmp stricmp
#else
#include <unistd.h>
#endif

extern unsigned char ComboHeader[23], ComboBlHeader[23], CombinDataGlob[3300];
extern unsigned char ShowTimer, savecfgforce;
extern unsigned int SnowTimer, NumSnow, NumComboGlob;
extern unsigned char GUIFontData1[705], GUIFontData[705];
enum vtype { UB, UW, UD, SB, SW, SD };

unsigned int ConvertBinaryToInt(char data[])
{
  int x;
  int num = 0;

  for(x = 0;x<8;x++) { if(data[x] == '1') { num |= BIT(7-x); } }

  return(num);
}

void InsertFontChar(char data[], int pos)
{
  GUIFontData1[pos] = ConvertBinaryToInt(data);
}

extern unsigned char newfont;

void LoadCustomFont()
{
  FILE *fp;
  char data[100];
  int x = 0;

  fp = fopen_dir(ZCfgPath, "zfont.txt", "r");
  if (fp)
  {
    while (fgets(data,100,fp) && strcmp(data,"EOF\n") && x < 705)
    {
      fgets(data,10,fp);		//get first line
      InsertFontChar(data,x++);

      fgets(data,10,fp);		//get second line
      InsertFontChar(data,x++);

      fgets(data,10,fp);		//get third line
      InsertFontChar(data,x++);

      fgets(data,10,fp);		//get fourth line
      InsertFontChar(data,x++);

      fgets(data,10,fp);		//get fifth line
      InsertFontChar(data,x++);
    }
  }
  else
  {
    memcpy(GUIFontData1,GUIFontData,705);
    fp = fopen_dir(ZCfgPath, "zfont.txt", "w");
    fputs("; empty space 0x00\n00000000\n00000000\n00000000\n00000000\n00000000\n",fp);
    fputs("; 0 0x01\n01110000\n10011000\n10101000\n11001000\n01110000\n",fp);
    fputs("; 1 0x02\n00100000\n01100000\n00100000\n00100000\n01110000\n",fp);
    fputs("; 2 0x03\n01110000\n10001000\n00110000\n01000000\n11111000\n",fp);
    fputs("; 3 0x04\n01110000\n10001000\n00110000\n10001000\n01110000\n",fp);
    fputs("; 4 0x05\n01010000\n10010000\n11111000\n00010000\n00010000\n",fp);
    fputs("; 5 0x06\n11111000\n10000000\n11110000\n00001000\n11110000\n",fp);
    fputs("; 6 0x07\n01110000\n10000000\n11110000\n10001000\n01110000\n",fp);
    fputs("; 7 0x08\n11111000\n00001000\n00010000\n00010000\n00010000\n",fp);
    fputs("; 8 0x09\n01110000\n10001000\n01110000\n10001000\n01110000\n",fp);
    fputs("; 9 0x0A\n01110000\n10001000\n01111000\n00001000\n01110000\n",fp);
    fputs("; A 0x0B\n01110000\n10001000\n11111000\n10001000\n10001000\n",fp);
    fputs("; B 0x0C\n11110000\n10001000\n11110000\n10001000\n11110000\n",fp);
    fputs("; C 0x0D\n01110000\n10001000\n10000000\n10001000\n01110000\n",fp);
    fputs("; D 0x0E\n11110000\n10001000\n10001000\n10001000\n11110000\n",fp);
    fputs("; E 0x0F\n11111000\n10000000\n11110000\n10000000\n11111000\n",fp);
    fputs("; F 0x10\n11111000\n10000000\n11110000\n10000000\n10000000\n",fp);
    fputs("; G 0x11\n01111000\n10000000\n10011000\n10001000\n01110000\n",fp);
    fputs("; H 0x12\n10001000\n10001000\n11111000\n10001000\n10001000\n",fp);
    fputs("; I 0x13\n11111000\n00100000\n00100000\n00100000\n11111000\n",fp);
    fputs("; J 0x14\n01111000\n00010000\n00010000\n10010000\n01100000\n",fp);
    fputs("; K 0x15\n10010000\n10100000\n11100000\n10010000\n10001000\n",fp);
    fputs("; L 0x16\n10000000\n10000000\n10000000\n10000000\n11111000\n",fp);
    fputs("; M 0x17\n11011000\n10101000\n10101000\n10101000\n10001000\n",fp);
    fputs("; N 0x18\n11001000\n10101000\n10101000\n10101000\n10011000\n",fp);
    fputs("; O 0x19\n01110000\n10001000\n10001000\n10001000\n01110000\n",fp);
    fputs("; P 0x1A\n11110000\n10001000\n11110000\n10000000\n10000000\n",fp);
    fputs("; Q 0x1B\n01110000\n10001000\n10101000\n10010000\n01101000\n",fp);
    fputs("; R 0x1C\n11110000\n10001000\n11110000\n10010000\n10001000\n",fp);
    fputs("; S 0x1D\n01111000\n10000000\n01110000\n00001000\n11110000\n",fp);
    fputs("; T 0x1E\n11111000\n00100000\n00100000\n00100000\n00100000\n",fp);
    fputs("; U 0x1F\n10001000\n10001000\n10001000\n10001000\n01110000\n",fp);
    fputs("; V 0x20\n10001000\n10001000\n01010000\n01010000\n00100000\n",fp);
    fputs("; W 0x21\n10001000\n10101000\n10101000\n10101000\n01010000\n",fp);
    fputs("; X 0x22\n10001000\n01010000\n00100000\n01010000\n10001000\n",fp);
    fputs("; Y 0x23\n10001000\n01010000\n00100000\n00100000\n00100000\n",fp);
    fputs("; Z 0x24\n11111000\n00010000\n00100000\n01000000\n11111000\n",fp);
    fputs("; - 0x25\n00000000\n00000000\n11111000\n00000000\n00000000\n",fp);
    fputs("; _ 0x26\n00000000\n00000000\n00000000\n00000000\n11111000\n",fp);
    fputs("; ~ 0x27\n01101000\n10010000\n00000000\n00000000\n00000000\n",fp);
    fputs("; . 0x28\n00000000\n00000000\n00000000\n00000000\n00100000\n",fp);
    fputs("; / 0x29\n00001000\n00010000\n00100000\n01000000\n10000000\n",fp);
    fputs("; < 0x2A\n00010000\n00100000\n01000000\n00100000\n00010000\n",fp);
    fputs("; > 0x2B\n01000000\n00100000\n00010000\n00100000\n01000000\n",fp);
    fputs("; [ 0x2C\n01110000\n01000000\n01000000\n01000000\n01110000\n",fp);
    fputs("; ] 0x2D\n01110000\n00010000\n00010000\n00010000\n01110000\n",fp);
    fputs("; : 0x2E\n00000000\n00100000\n00000000\n00100000\n00000000\n",fp);
    fputs("; & 0x2F\n01100000\n10011000\n01110000\n10011000\n01101000\n",fp);
    fputs("; arrow down 0x30\n00100000\n00100000\n10101000\n01110000\n00100000\n",fp);
    fputs("; # 0x31\n01010000\n11111000\n01010000\n11111000\n01010000\n",fp);
    fputs("; = 0x32\n00000000\n11111000\n00000000\n11111000\n00000000\n",fp);
    fputs("; \" 0x33\n01001000\n10010000\n00000000\n00000000\n00000000\n",fp);
    fputs("; \\ 0x34\n10000000\n01000000\n00100000\n00010000\n00001000\n",fp);
    fputs("; * 0x35\n10101000\n01110000\n11111000\n01110000\n10101000\n",fp);
    fputs("; ? 0x36\n01110000\n10001000\n00110000\n00000000\n00100000\n",fp);
    fputs("; % 0x37\n10001000\n00010000\n00100000\n01000000\n10001000\n",fp);
    fputs("; + 0x38\n00100000\n00100000\n11111000\n00100000\n00100000\n",fp);
    fputs("; , 0x39\n00000000\n00000000\n00000000\n00100000\n01000000\n",fp);
    fputs("; ( 0x3A\n00110000\n01000000\n01000000\n01000000\n00110000\n",fp);
    fputs("; ) 0x3B\n01100000\n00010000\n00010000\n00010000\n01100000\n",fp);
    fputs("; @ 0x3C\n01110000\n10011000\n10111000\n10000000\n01110000\n",fp);
    fputs("; \' 0x3D\n00100000\n01000000\n00000000\n00000000\n00000000\n",fp);
    fputs("; ! 0x3E\n00100000\n00100000\n00100000\n00000000\n00100000\n",fp);
    fputs("; $ 0x3F\n01111000\n10100000\n01110000\n00101000\n11110000\n",fp);
    fputs("; ; 0x40\n00000000\n00100000\n00000000\n00100000\n01000000\n",fp);
    fputs("; ` 0x41\n01000000\n00100000\n00000000\n00000000\n00000000\n",fp);
    fputs("; ^ 0x42\n00100000\n01010000\n00000000\n00000000\n00000000\n",fp);
    fputs("; { 0x43\n00110000\n01000000\n11000000\n01000000\n00110000\n",fp);
    fputs("; } 0x44\n01100000\n00010000\n00011000\n00010000\n01100000\n",fp);
    fputs("; up 0x45\n00100000\n00100000\n01110000\n01110000\n11111000\n",fp);
    fputs("; down 0x46\n11111000\n01110000\n01110000\n00100000\n00100000\n",fp);
    fputs("; left 0x47\n00001000\n00111000\n11111000\n00111000\n00001000\n",fp);
    fputs("; right 0x48\n10000000\n11100000\n11111000\n11100000\n10000000\n",fp);
    fputs("; arrow left 0x49\n00100000\n01100000\n11111000\n01100000\n00100000\n",fp);
    fputs("; .5 0x4A\n00111000\n00100000\n00110000\n00001000\n10110000\n",fp);
    fputs("; maximize (Win) 0x4B\n11111100\n10000100\n11111100\n00000000\n00000000\n",fp);
    fputs("; minimize (Win) 0x4C\n00000000\n11111100\n00000000\n00000000\n00000000\n",fp);
    fputs("; maximize (SDL) 0x4D\n11111000\n10001000\n10001000\n10001000\n11111000\n",fp);
    fputs("; shw fullstop 0x4E\n00000000\n00000000\n00100000\n01010000\n00100000\n",fp);
    fputs("; shw left bracket 0x4F\n01110000\n01000000\n01000000\n01000000\n00000000\n",fp);
    fputs("; shw right bracket 0x50\n00000000\n00010000\n00010000\n00010000\n01110000\n",fp);
    fputs("; shw comma 0x51\n00000000\n00000000\n00000000\n01000000\n00100000\n",fp);
    fputs("; shw mid-dot 0x52\n00000000\n00100000\n01110000\n00100000\n00000000\n",fp);
    fputs("; shw wo 0x53\n11111000\n00001000\n11110000\n00100000\n11000000\n",fp);
    fputs("; shw mini a 0x54\n00000000\n11111000\n01010000\n01100000\n01000000\n",fp);
    fputs("; shw mini i 0x55\n00000000\n00010000\n00100000\n11100000\n00100000\n",fp);
    fputs("; shw mini u 0x56\n00000000\n00100000\n11111000\n10001000\n00110000\n",fp);
    fputs("; shw mini e 0x57\n00000000\n00000000\n11111000\n00100000\n11111000\n",fp);
    fputs("; shw mini o 0x58\n00000000\n00010000\n11111000\n00110000\n11010000\n",fp);
    fputs("; shw mini ya 0x59\n00000000\n01000000\n11111000\n01010000\n01000000\n",fp);
    fputs("; shw mini yu 0x5A\n00000000\n00000000\n11110000\n00010000\n11111000\n",fp);
    fputs("; shw mini yo 0x5B\n00000000\n11111000\n00001000\n01111000\n11111000\n",fp);
    fputs("; shw mini tsu 0x5C\n00000000\n10101000\n10101000\n00010000\n01100000\n",fp);
    fputs("; shw prolong 0x5D\n00000000\n10000000\n01111000\n00000000\n00000000\n",fp);
    fputs("; shw a 0x5E\n11111000\n00101000\n00110000\n00100000\n11000000\n",fp);
    fputs("; shw i 0x5F\n00001000\n00110000\n11100000\n00100000\n00100000\n",fp);
    fputs("; shw u 0x60\n00100000\n11111000\n10001000\n00010000\n01100000\n",fp);
    fputs("; shw e 0x61\n11111000\n00100000\n00100000\n00100000\n11111000\n",fp);
    fputs("; shw o 0x62\n00010000\n11111000\n00110000\n01010000\n10010000\n",fp);
    fputs("; shw ka 0x63\n01000000\n11111000\n01001000\n01001000\n10011000\n",fp);
    fputs("; shw ki 0x64\n00100000\n11111000\n00100000\n11111000\n00100000\n",fp);
    fputs("; shw ku 0x65\n01000000\n01111000\n10001000\n00010000\n01100000\n",fp);
    fputs("; shw ke 0x66 ^^\n01000000\n01111000\n10010000\n00010000\n01100000\n",fp);
    fputs("; shw ko 0x67\n11111000\n00001000\n00001000\n00001000\n11111000\n",fp);
    fputs("; shw sa 0x68\n01010000\n11111000\n01010000\n00010000\n01100000\n",fp);
    fputs("; shw shi 0x69\n01000000\n10101000\n01001000\n00010000\n11100000\n",fp);
    fputs("; shw su 0x6A\n11111000\n00001000\n00010000\n00110000\n11001000\n",fp);
    fputs("; shw se 0x6B\n01000000\n11111000\n01010000\n01000000\n00111000\n",fp);
    fputs("; shw so 0x6C\n10001000\n01001000\n00001000\n00010000\n01100000\n",fp);
    fputs("; shw ta 0x6D\n01000000\n01111000\n11001000\n00110000\n01100000\n",fp);
    fputs("; shw chi 0x6E\n11111000\n00100000\n11111000\n00100000\n01000000\n",fp);
    fputs("; shw tsu 0x6F\n10101000\n10101000\n00001000\n00010000\n01100000\n",fp);
    fputs("; shw te 0x70\n11111000\n00000000\n11111000\n00100000\n11000000\n",fp);
    fputs("; shw to 0x71\n01000000\n01000000\n01100000\n01010000\n01000000\n",fp);
    fputs("; shw na 0x72\n00100000\n11111000\n00100000\n00100000\n01000000\n",fp);
    fputs("; shw ni 0x73\n11110000\n00000000\n00000000\n00000000\n11111000\n",fp);
    fputs("; shw nu 0x74\n11111000\n00001000\n00101000\n00010000\n01101000\n",fp);
    fputs("; shw ne 0x75\n00100000\n11111000\n00001000\n01110000\n10101000\n",fp);
    fputs("; shw no 0x76\n00001000\n00001000\n00001000\n00010000\n01100000\n",fp);
    fputs("; shw ha 0x77\n01010000\n01010000\n01010000\n10001000\n10001000\n",fp);
    fputs("; shw hi 0x78\n10000000\n10011000\n11100000\n10000000\n01111000\n",fp);
    fputs("; shw hu 0x79\n11111000\n00001000\n00001000\n00010000\n01100000\n",fp);
    fputs("; shw he 0x7A\n01000000\n10100000\n10010000\n00001000\n00000000\n",fp);
    fputs("; shw ho 0x7B\n00100000\n11111000\n01110000\n10101000\n00100000\n",fp);
    fputs("; shw ma 0x7C\n11111000\n00001000\n10010000\n01100000\n00100000\n",fp);
    fputs("; shw mi 0x7D\n11111000\n00000000\n11111000\n00000000\n11111000\n",fp);
    fputs("; shw mu 0x7E\n00100000\n01000000\n01000000\n10010000\n11111000\n",fp);
    fputs("; shw me 0x7F\n00001000\n01001000\n00110000\n00110000\n11001000\n",fp);
    fputs("; shw mo 0x80\n11111000\n00100000\n11111000\n00100000\n00111000\n",fp);
    fputs("; shw ya 0x81\n01000000\n11111100\n01001000\n00100000\n00100000\n",fp);
    fputs("; shw yu 0x82\n11110000\n00010000\n00010000\n00010000\n11111000\n",fp);
    fputs("; shw yo 0x83\n11111000\n00001000\n11111000\n00001000\n11111000\n",fp);
    fputs("; shw ra 0x84\n11111000\n00000000\n11111000\n00010000\n01100000\n",fp);
    fputs("; shw ri 0x85\n10001000\n10001000\n10001000\n00010000\n01100000\n",fp);
    fputs("; shw ru 0x86\n01100000\n01100000\n01101000\n01101000\n10110000\n",fp);
    fputs("; shw re 0x87\n10000000\n10000000\n10001000\n10001000\n11110000\n",fp);
    fputs("; shw ro 0x88\n11111000\n10001000\n10001000\n10001000\n11111000\n",fp);
    fputs("; shw wa 0x89\n11111000\n10001000\n00001000\n00010000\n01100000\n",fp);
    fputs("; shw n 0x8A\n10000000\n01001000\n00001000\n00010000\n11100000\n",fp);
    fputs("; shw voiced 0x8B\n10100000\n10100000\n00000000\n00000000\n00000000\n",fp);
    fputs("; shw halfvoiced 0x8C\n01000000\n10100000\n01000000\n00000000\n00000000\n",fp);
    fputs("EOF\n",fp);
  }

  fclose(fp);
}

static void CheckValueBounds(void *ptr, int min, int max, int val, enum vtype type)
{
  switch (type)
  {
    case SB:
      if (((*(char*)ptr) > (char)max) || ((*(char*)ptr) < (char)min))
      { *(char*)ptr = (char)val; }
      break;
    case UB:
      if (((*(unsigned char*)ptr) > (unsigned char)max) ||
          ((*(unsigned char*)ptr) < (unsigned char)min))
      { *(unsigned char*)ptr = (unsigned char)val; }
      break;

    case SW:
      if (((*(short*)ptr) > (short)max) || ((*(short*)ptr) < (short)min))
      { *(short*)ptr = (short)val; }
      break;
    case UW:
      if (((*(unsigned short*)ptr) > (unsigned short)max) ||
          ((*(unsigned short*)ptr) < (unsigned short)min))
      { *(unsigned short*)ptr = (unsigned short)val; }
      break;

    default:
    case SD:
      if (((*(int*)ptr) > max) || ((*(int*)ptr) < min))
      { *(int*)ptr = val; }
      break;
    case UD:
      if (((*(unsigned int*)ptr) > (unsigned int)max) ||
          ((*(unsigned int*)ptr) < (unsigned int)min))
      { *(unsigned int*)ptr = (unsigned int)val; }
  }
}

unsigned char CalcCfgChecksum()
{
  unsigned char *ptr = &GUIRAdd, i = 0;
  unsigned short chksum = 0;

  for (; i < 100 ; i++, ptr++)  { chksum += *ptr; }

  chksum ^= 0xB2ED; // xor bx,1011001011101101b
  i = (chksum & 0x800) >> 8;
  chksum &= 0xF7FF; // and bh,0F7h

  if (chksum & 0x10) { chksum |= 0x800; }
  chksum &= 0xFFEF; // and bl,0EFh
  if (i) { chksum |= 0x10; }

  i = (chksum >> 8);

  return (((chksum & 0xFF) ^ i) | 0x80);
}

void GUIRestoreVars()
{
  int i;
  unsigned char read_cfg_vars(const char *);
  FILE *cfg_fp;

  char *path = strdupcat(ZCfgPath, ZCfgFile);
  if (path)
  {
    read_cfg_vars(path);
    free(path);
  }
  else
  {
    read_cfg_vars(ZCfgFile);
  }

  path = strdupcat(ZCfgPath, "zmovie.cfg");
  if (path)
  {
    read_md_vars(path);
    free(path);
  }
  else
  {
    read_md_vars("zmovie.cfg");
  }

  CheckValueBounds(&per2exec, 50, 150, 100, UD);
  CheckValueBounds(&SRAMSave5Sec, 0, 1, 0, UB);
  CheckValueBounds(&OldGfxMode2, 0, 1, 0, UB);

#ifdef __MSDOS__
  CheckValueBounds(&pl1contrl, 0, 15, 1, UB);
  CheckValueBounds(&pl1p209, 0, 1, 0, UB);
  CheckValueBounds(&pl2contrl, 0, 15, 0, UB);
  CheckValueBounds(&pl2p209, 0, 1, 0, UB);
  CheckValueBounds(&pl3contrl, 0, 15, 0, UB);
  CheckValueBounds(&pl3p209, 0, 1, 0, UB);
  CheckValueBounds(&pl4contrl, 0, 15, 0, UB);
  CheckValueBounds(&pl4p209, 0, 1, 0, UB);
  CheckValueBounds(&pl5contrl, 0, 15, 0, UB);
  CheckValueBounds(&pl5p209, 0, 1, 0, UB);
#else
  CheckValueBounds(&pl1contrl, 0, 1, 1, UB);
  CheckValueBounds(&pl2contrl, 0, 1, 0, UB);
  CheckValueBounds(&pl3contrl, 0, 1, 0, UB);
  CheckValueBounds(&pl4contrl, 0, 1, 0, UB);
  CheckValueBounds(&pl5contrl, 0, 1, 0, UB);
#endif

#ifdef __UNIXSDL__
  CheckValueBounds(&joy_sensitivity, 0, 32767, 16384, UW);
#endif

  CheckValueBounds(&pl12s34, 0, 1, 0, UB);
  CheckValueBounds(&AllowUDLR, 0, 1, 0, UB);
#ifdef __MSDOS__
  CheckValueBounds(&SidewinderFix, 0, 1, 0, UB);
#endif

#ifdef __WIN32__
  CheckValueBounds(&cvidmode, 0, 41, 3, UB);
  CheckValueBounds(&PrevWinMode, 0, 41, 3, UB);
  CheckValueBounds(&PrevFSMode, 0, 41, 6, UB);
#endif
#ifdef __UNIXSDL__
#ifdef __OPENGL__
  CheckValueBounds(&cvidmode, 0, 23, 2, UB);
  CheckValueBounds(&PrevWinMode, 0, 23, 2, UB);
  CheckValueBounds(&PrevFSMode, 0, 23, 3, UB);
#else
  CheckValueBounds(&cvidmode, 0, 5, 2, UB);
  CheckValueBounds(&PrevWinMode, 0, 5, 2, UB);
  CheckValueBounds(&PrevFSMode, 0, 5, 3, UB);
#endif
#endif
#ifdef __MSDOS__
  CheckValueBounds(&cvidmode, 0, 18, 4, UB);
#endif
  CheckValueBounds(&CustomResX, 256, 2048, 640, UD);
  CheckValueBounds(&CustomResY, 224, 1536, 480, UD);
  CheckValueBounds(&newengen, 0, 1, 1, UB);
  CheckValueBounds(&scanlines, 0, 3, 0, UB);
  CheckValueBounds(&antienab, 0, 1, 0, UB);
#ifndef __UNIXSDL__
  CheckValueBounds(&vsyncon, 0, 1, 0, UB);
#endif
#ifdef __WIN32__
  CheckValueBounds(&TripleBufferWin, 0, 1, 0, UB);
#endif
#ifdef __MSDOS__
  CheckValueBounds(&smallscreenon, 0, 1, 0, UD);
  CheckValueBounds(&ScreenScale, 0, 1, 0, UB);
  CheckValueBounds(&Triplebufen, 0, 1, 0, UB);
#endif
#ifdef __UNIXSDL__
  CheckValueBounds(&BilinearFilter, 0, 1, 0, UB);
#endif
  CheckValueBounds(&En2xSaI, 0, 3, 0, UB);
#ifndef __MSDOS__
  CheckValueBounds(&hqFilter, 0, 1, 0, UB);
#endif
  CheckValueBounds(&GrayscaleMode, 0, 1, 0, UB);
  CheckValueBounds(&Mode7HiRes16b, 0, 1, 0, UD);
  CheckValueBounds(&NTSCFilter, 0, 1, 0, UB);
  CheckValueBounds(&NTSCBlend, 0, 1, 0, UB);
  CheckValueBounds(&NTSCHue, -100, 100, 0, SB);
  CheckValueBounds(&NTSCSat, -100, 100, 0, SB);
  CheckValueBounds(&NTSCCont, -100, 100, 0, SB);
  CheckValueBounds(&NTSCBright, -100, 100, 0, SB);
  CheckValueBounds(&NTSCSharp, -100, 100, 0, SB);
  CheckValueBounds(&NTSCWarp, -100, 100, 0, SB);
  CheckValueBounds(&NTSCRef, 0, 1, 0, UB);

  CheckValueBounds(&soundon, 0, 1, 1, UB);
  CheckValueBounds(&StereoSound, 0, 1, 1, UB);
  CheckValueBounds(&SoundQuality, 0, 6, 5, UD);
  CheckValueBounds(&MusicRelVol, 0, 100, 100, UB);
  CheckValueBounds(&RevStereo, 0, 1, 0, UB);
#ifdef __MSDOS__
  CheckValueBounds(&Force8b, 0, 1, 0, UB);
#endif
  CheckValueBounds(&SPCDisable, 0, 1, 0, UB);
  CheckValueBounds(&EchoDis, 0, 1, 0, UB);
  CheckValueBounds(&SoundBufEn, 0, 1, 0, UB);
  CheckValueBounds(&Surround, 0, 1, 0, UB);
  CheckValueBounds(&SoundInterpType, 0, 3, 1, UB);
  CheckValueBounds(&LowPassFilterType, 0, 3, 0, UB);
#ifdef __WIN32__
  CheckValueBounds(&PrimaryBuffer, 0, 1, 0, UB);
#endif

  CheckValueBounds(&frameskip, 0, 10, 0, UB);
  CheckValueBounds(&maxskip, 0, 9, 9, UB);
  CheckValueBounds(&EmuSpeed, 0, 58, 29, UB);
  CheckValueBounds(&Turbo30hz, 0, 1, 1, UB);
  CheckValueBounds(&FastFwdToggle, 0, 1, 0, UB);
  CheckValueBounds(&FFRatio, 0, 28, 8, UB);
  CheckValueBounds(&SDRatio, 0, 28, 0, UB);
  CheckValueBounds(&SRAMState, 0, 1, 1, UB);
  CheckValueBounds(&AutoIncSaveSlot, 0, 1, 0, UB);
  CheckValueBounds(&LatestSave, 0, 1, 0, UB);
  CheckValueBounds(&AutoState, 0, 1, 0, UB);
  CheckValueBounds(&RewindStates, 0, 99, 8, UB);
  CheckValueBounds(&RewindFrames, 1, 99, 15, UB);
#ifndef __UNIXSDL__
  CheckValueBounds(LoadDrive, 0, 25, 2, UB);
#endif
#ifdef NO_PNG
  CheckValueBounds(&ScreenShotFormat, 0, 0, 0, UB);
#else
  CheckValueBounds(&ScreenShotFormat, 0, 1, 0, UB);
#endif
  CheckValueBounds(&MMXSupport, 0, 1, 1, UB);
  CheckValueBounds(&SmallMsgText, 0, 1, 0, UB);
  CheckValueBounds(&GUIEnableTransp, 0, 1, 0, UB);
  CheckValueBounds(&PauseLoad, 0, 1, 0, UB);
  CheckValueBounds(&PauseRewind, 0, 1, 0, UB);
  CheckValueBounds(&FPSAtStart, 0, 1, 0, UB);
  CheckValueBounds(&TimerEnable, 0, 1, 0, UB);
  CheckValueBounds(&TwelveHourClock, 0, 1, 0, UB);
  CheckValueBounds(&AutoLoadCht, 0, 1, 0, UB);
  CheckValueBounds(&AutoPatch, 0, 1, 1, UB);
  CheckValueBounds(&PauseFocusChange, 0, 1, 0, UB);
  CheckValueBounds(&DisplayInfo, 0, 1, 1, UB);
  CheckValueBounds(&RomInfo, 0, 1, 1, UB);
#ifdef __WIN32__
  CheckValueBounds(&HighPriority, 0, 1, 0, UB);
  CheckValueBounds(&SaveMainWindowPos, 0, 1, 1, UB);
  CheckValueBounds(&AllowMultipleInst, 0, 1, 1, UB);
  CheckValueBounds(&DisableScreenSaver, 0, 1, 1, UB);
#endif
  CheckValueBounds(&cfgdontsave, 0, 1, 0, UB);
  CheckValueBounds(&FirstTimeData, 0, 1, 1, UB);

  CheckValueBounds(&guioff, 0, 1, 0, UB);
  CheckValueBounds(&showallext, 0, 1, 0, UB);
#ifdef __MSDOS__
  CheckValueBounds(&GUIloadfntype, 0, 2, 0, UB);
#endif
  CheckValueBounds(&prevlfreeze, 0, 1, 0, UB);
  CheckValueBounds(&GUIRAdd, 0, 31, 15, UB);
  CheckValueBounds(&GUIGAdd, 0, 31, 10, UB);
  CheckValueBounds(&GUIBAdd, 0, 31, 31, UB);
  CheckValueBounds(&GUITRAdd, 0, 31, 0, UB);
  CheckValueBounds(&GUITGAdd, 0, 31, 10, UB);
  CheckValueBounds(&GUITBAdd, 0, 31, 31, UB);
  CheckValueBounds(&GUIWRAdd, 0, 31, 8, UB);
  CheckValueBounds(&GUIWGAdd, 0, 31, 8, UB);
  CheckValueBounds(&GUIWBAdd, 0, 31, 25, UB);
  CheckValueBounds(&GUIEffect, 0, 4, 0, UB);
  CheckValueBounds(&FilteredGUI, 0, 1, 1, UB);
  CheckValueBounds(&mousewrap, 0, 1, 0, UB);
  CheckValueBounds(&mouseshad, 0, 1, 1, UB);
  CheckValueBounds(&esctomenu, 0, 1, 1, UB);
  CheckValueBounds(&resetposn, 0, 1, 1, UB);
  for (i=1 ; i<22 ; i++)
  {
    CheckValueBounds(GUIwinposx+i, -233, 254, 10, SD);
    CheckValueBounds(GUIwinposy+i, 8, 221, 20, SD);
  }
#ifdef __WIN32__
  CheckValueBounds(&MouseWheel, 0, 1, 1, UB);
  CheckValueBounds(&TrapMouseCursor, 0, 1, 0, UB);
  CheckValueBounds(&AlwaysOnTop, 0, 1, 0, UB);
#endif
  CheckValueBounds(&GUIComboGameSpec, 0, 1, 0, UB);
  CheckValueBounds(&GUIClick, 0, 1, 0, UB);
  CheckValueBounds(&JoyPad1Move, 0, 1, 0, UB);

  CheckValueBounds(&CheatSrcByteSize, 0, 3, 0, UB);
  CheckValueBounds(&CheatSrcByteBase, 0, 1, 0, UB);
  CheckValueBounds(&CheatSrcSearchType, 0, 1, 0, UB);
  CheckValueBounds(&CheatUpperByteOnly, 0, 1, 0, UB);

  CheckValueBounds(&MovieDisplayFrame, 0, 1, 0, UB);
  CheckValueBounds(&MovieStartMethod, 0, 3, 0, UB);
  CheckValueBounds(&MovieVideoMode, 0, 5, 4, UB);
  CheckValueBounds(&MovieAudio, 0, 1, 1, UB);
  CheckValueBounds(&MovieVideoAudio, 0, 1, 1, UB);
  CheckValueBounds(&MovieAudioCompress, 0, 1, 1, UB);
#ifdef __MSDOS__
  CheckValueBounds(&DisplayS, 0, 1, 0, UB);
  CheckValueBounds(&Palette0, 0, 1, 1, UB);
#endif
#ifdef __WIN32__
  CheckValueBounds(&KitchenSync, 0, 1, 0, UB);
  CheckValueBounds(&KitchenSyncPAL, 0, 1, 0, UB);
  CheckValueBounds(&ForceRefreshRate, 0, 1, 0, UB);
  CheckValueBounds(&SetRefreshRate, 60, 180, 60, UB);
  CheckValueBounds(&Keep4_3Ratio, 0, 1, 1, UB);
#endif

  if (TimeChecker == CalcCfgChecksum())
  {
    ShowTimer = 1;
    NumSnow = 200;
    SnowTimer = 0;
  }

  NumComboGlob = 0;

  if ((cfg_fp = fopen_dir(ZCfgPath, "data.cmb", "rb")))
  {
    fread(ComboBlHeader, 1, 23, cfg_fp);

    if (ComboBlHeader[22])
    {
      NumComboGlob = ComboBlHeader[22];
      fread(CombinDataGlob, 1, 66*NumComboGlob, cfg_fp);
    }

    fclose(cfg_fp);
  }

  LoadCustomFont();
}

void GUISaveVars()
{
  unsigned char write_cfg_vars(const char *);
  FILE *cfg_fp;

  if (ShowTimer == 1) { TimeChecker = CalcCfgChecksum(); }

  if (!cfgdontsave || savecfgforce)
  {
    char *path = strdupcat(ZCfgPath, ZCfgFile);
    if (path)
    {
      swap_backup_vars();
      write_cfg_vars(path);
      free(path);
      swap_backup_vars();
    }
  }

  if (NumComboGlob && (cfg_fp = fopen_dir(ZCfgPath, "data.cmb", "wb")))
  {
    ComboHeader[22] = NumComboGlob;
    fwrite(ComboHeader, 1, 23, cfg_fp);
    fwrite(CombinDataGlob, 1, 66*NumComboGlob, cfg_fp);
    fclose(cfg_fp);
  }
}

//~81 prior to solar peak, horizontal compensation needs to be made.
//ISBN-014036336X in the second to last chapter discusses how emulating bonjour results in a special card case.
//Thanks Motley!
unsigned int horizon[][4][8] = {{{0x6F746E41, 0x57656E69, 0x61772047, 0x65682073, 0x00216572, 0xB7CE8EB8, 0x00000006, 0x00000011},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x746E6153, 0x6F632061, 0x2073656D, 0x6E656877, 0x20746920, 0x776F6E73, 0x00000073, 0x00000011},
                                 {0x6F666562, 0x74206572, 0x6E206568, 0x79207765, 0x2E726165, 0x776F6E00, 0x00000073, 0x00000011},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x6E64694D, 0x74686769, 0x6D615620, 0x65726970, 0x6C662073, 0x77006565, 0x00000073, 0x00000011},
                                 {0x6F666562, 0x5A206572, 0x53454E53, 0x646E6120, 0x27746920, 0x77000073, 0x00000073, 0x00000011},
                                 {0x746E6F63, 0x206C6F72, 0x6720666F, 0x696C7261, 0x6F742063, 0x2E747361, 0x00000000, 0x00000011},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x454E535A, 0x61682053, 0x65642073, 0x74636574, 0x74206465, 0x00746168, 0x00000000, 0x00000011},
                                 {0x20756F79, 0x20646964, 0x20746F6E, 0x616E6F64, 0x74206574, 0x7961646F, 0x0000002E, 0x00000011},
                                 {0x20756F59, 0x6C6C6977, 0x776F6E20, 0x70786520, 0x65697265, 0x0065636E, 0x0000002E, 0x00000011},
                                 {0x2072756F, 0x74617277, 0x77002E68, 0x70786520, 0x65697265, 0x0065636E, 0x0000002E, 0x00000011}},

                                {{0x72756F59, 0x454E5320, 0x6F642053, 0x6E207365, 0x7320746F, 0x006D6565, 0x0000002E, 0x00000011},
                                 {0x62206F74, 0x6C702065, 0x65676775, 0x6E692064, 0x79206F74, 0x0072756F, 0x0000002E, 0x00000011},
                                 {0x656C6554, 0x69736976, 0x70206E6F, 0x65706F72, 0x2E796C72, 0x00727500, 0x0000002E, 0x00000011},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x7E206E49, 0x64203138, 0x20737961, 0x6F732061, 0x0072616C, 0x00727500, 0x0000002E, 0xB7F261E0},
                                 {0x65776F70, 0x20646572, 0x454E535A, 0x69772053, 0x62206C6C, 0x74612065, 0x00000000, 0xB7F261E0},
                                 {0x73277469, 0x61657020, 0x4500216B, 0x69772053, 0x62206C6C, 0x74612065, 0x00000000, 0xB7F261E0},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x61206557, 0x6E206572, 0x7220776F, 0x726F7065, 0x676E6974, 0x756F7920, 0x00210072, 0x00000011},
                                 {0x696D6167, 0x6120676E, 0x76697463, 0x65697469, 0x6F742073, 0x756F7900, 0x00210072, 0x00000011},
                                 {0x746E694E, 0x6F646E65, 0x63207327, 0x72746E65, 0x73206C61, 0x65767265, 0x00007372, 0x00000011},
                                 {0x61656C70, 0x77206573, 0x20746961, 0x6F6D2061, 0x746E656D, 0x6576002E, 0x00007372, 0x00000011}},

                                {{0x20657241, 0x20756F79, 0x72616568, 0x20676E69, 0x00796E61, 0x6576002E, 0x00007372, 0x00000011},
                                 {0x63696F76, 0x69207365, 0x6F79206E, 0x68207275, 0x00646165, 0x6576002E, 0x00007372, 0x00000011},
                                 {0x68676972, 0x6F6E2074, 0x6F003F77, 0x68207275, 0x00646165, 0x6576002E, 0x00007372, 0x00000011},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x73277449, 0x746F6E20, 0x6F6F7420, 0x74616C20, 0x00640065, 0x6576002E, 0x00007372, 0x00000011},
                                 {0x65766E69, 0x69207473, 0x535A206E, 0x2053454E, 0x61646F74, 0x65002179, 0x00007372, 0x00000011},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x20646944, 0x20756F79, 0x776F6E6B, 0x20666920, 0x00756F79, 0x65002179, 0x00007372, 0x00000011},
                                 {0x20797562, 0x65676170, 0x6C756166, 0x65622074, 0x00007265, 0x65002179, 0x00007372, 0x00000011},
                                 {0x20756F79, 0x6C6C6977, 0x6B616D20, 0x69682065, 0x0000006D, 0x65002179, 0x00007372, 0x00000011},
                                 {0x70706168, 0x6C003F79, 0x6B616D20, 0x69682065, 0x0000006D, 0x65002179, 0x00007372, 0x00000011}},

                                {{0x276E6F44, 0x6F792074, 0x65662075, 0x74206C65, 0x69727265, 0x00656C62, 0x00007372, 0x00000011},
                                 {0x776F6E6B, 0x20676E69, 0x20756F79, 0x20657375, 0x454E535A, 0x00650053, 0x00007372, 0x00000011},
                                 {0x20646E61, 0x65766168, 0x2074276E, 0x616E6F64, 0x00646574, 0x00650053, 0x00007372, 0x00000011},
                                 {0x756F6E65, 0x74206867, 0x7261776F, 0x69207364, 0x00003F74, 0x00650053, 0x00007372, 0x00000011}},

                                {{0x20796857, 0x20657261, 0x20756F79, 0x79616C70, 0x00676E69, 0x00650053, 0x00007372, 0x00000011},
                                 {0x656D6167, 0x68772073, 0x79206E65, 0x7320756F, 0x6C756F68, 0x00650064, 0x00007372, 0x00000011},
                                 {0x73206562, 0x646E6570, 0x20676E69, 0x6C617571, 0x20797469, 0x656D6974, 0x00007300, 0x00000011},
                                 {0x68746977, 0x756F7920, 0x61662072, 0x796C696D, 0x2079003F, 0x656D6974, 0x00007300, 0x00000011}},

                                {{0x73277449, 0x73656220, 0x6F742074, 0x616C7020, 0x20790079, 0x656D6974, 0x00007300, 0x00000011},
                                 {0x53454E53, 0x6D616720, 0x77207365, 0x656C6968, 0x61657720, 0x676E6972, 0x00007300, 0x00000011},
                                 {0x69786F62, 0x6720676E, 0x65766F6C, 0x65002E73, 0x61657720, 0x676E6972, 0x00007300, 0x00000011},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x79206F44, 0x7420756F, 0x6B6E6968, 0x69737520, 0x6100676E, 0x676E6972, 0x00007300, 0x00000011},
                                 {0x454E535A, 0x6E692053, 0x61657263, 0x20736573, 0x72756F79, 0x676E6900, 0x00007300, 0x00000011},
                                 {0x69736564, 0x74206572, 0x7573206F, 0x726F7070, 0x72750074, 0x676E6900, 0x00007300, 0x00000011},
                                 {0x65766564, 0x6D706F6C, 0x3F746E65, 0x726F7000, 0x72750074, 0x676E6900, 0x00007300, 0x00000011}},

                                {{0x6E616854, 0x6F79206B, 0x6F662075, 0x6C702072, 0x6E697961, 0x676E0067, 0x00007300, 0x00000011},
                                 {0x73657270, 0x65746E65, 0x79622064, 0x6C702000, 0x6E697961, 0x676E0067, 0x00007300, 0x00000011},
                                 {0x454E535A, 0x65742053, 0x00216D61, 0x6C702000, 0x6E697961, 0x676E0067, 0x00007300, 0x00000011},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x20796857, 0x20646964, 0x20756F79, 0x64616F6C, 0x4E535A20, 0x67005345, 0x00007300, 0x00000011},
                                 {0x3F726F66, 0x79725420, 0x6F6E6120, 0x72656874, 0x4E535A00, 0x67005345, 0x00007300, 0x00000011},
                                 {0x53454E53, 0x756D6520, 0x6F74616C, 0x72002E72, 0x4E535A00, 0x67005345, 0x00007300, 0x00000011},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x61656C50, 0x73206573, 0x206E6163, 0x72756F79, 0x4E535A00, 0x67005345, 0x00007300, 0x00000011},
                                 {0x706D6F63, 0x72657475, 0x726F6620, 0x72697620, 0x73657375, 0x67000021, 0x00007300, 0x00000011},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x74206557, 0x6B6E6968, 0x756F7920, 0x6F632072, 0x7475706D, 0x67007265, 0x00007300, 0x00000011},
                                 {0x65746168, 0x6F792073, 0x42202175, 0x66612065, 0x64696172, 0x67000021, 0x00007300, 0x00000011},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x20646944, 0x20756F79, 0x776F6E6B, 0x6C206120, 0x65677261, 0x67000000, 0x00007300, 0x00000011},
                                 {0x63726570, 0x20746E65, 0x5A20666F, 0x53454E53, 0x73617720, 0x67000000, 0x00007300, 0x00000011},
                                 {0x61657263, 0x20646574, 0x61207962, 0x73696620, 0x73003F68, 0x67000000, 0x00007300, 0x00000011},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

                                {{0x6E6E6957, 0x20737265, 0x276E6F64, 0x73752074, 0x72642065, 0x2E736775, 0x00007300, 0xB7F1F1E0},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
                                 {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},
#ifndef __UNIXSDL__
                                {{0x27756F59, 0x73206572, 0x6C6C6974, 0x69737520, 0x6120676E, 0x002E7300, 0x00007300, 0x00000011},
                                 {0x7263694D, 0x666F736F, 0x534F2074, 0x6547203F, 0x69772074, 0x00006874, 0x00007300, 0x00000011},
                                 {0x20656874, 0x676F7270, 0x2C6D6172, 0x69777320, 0x20686374, 0x00006F74, 0x00007300, 0x00000011},
                                 {0x756E694C, 0x726F2078, 0x44534220, 0x6977002E, 0x20686374, 0x00006F74, 0x00007300, 0x00000011}}};
#else
                                {{0x656D6F43, 0x2C6E6F20, 0x65737520, 0x72206120, 0x006C6165, 0x00006F74, 0x00007300, 0x00000011},
                                 {0x7265704F, 0x6E697461, 0x79532067, 0x6D657473, 0x6B696C20, 0x00000065, 0x00007300, 0x00000011},
                                 {0x646E6957, 0x2C73776F, 0x6F747320, 0x65622070, 0x00676E69, 0x00000065, 0x00007300, 0x00000011},
                                 {0x66666964, 0x6E657265, 0x6F002E74, 0x65622070, 0x00676E69, 0x00000065, 0x00007300, 0x00000011}}};
#endif

unsigned int *horizon_get(unsigned int distance)
{
  return(horizon[distance%21][0]);
}

extern unsigned int GUICBHold, NumCheats, statefileloc;
extern unsigned char cheatdata[28*255+56];

void CheatCodeSave()
{
  FILE *fp = 0;
  unsigned int size = 0;

  GUICBHold=0;

  if (NumCheats)
  {
    cheatdata[6]=254;
    cheatdata[7]=252;

    setextension(ZSaveName, "cht");

    if ((fp = fopen_dir(ZSramPath,ZSaveName,"wb")))
    {
      size=(NumCheats<<4)+3*(NumCheats<<2);
      fwrite(cheatdata, 1, size, fp);
      fclose(fp);
    }
  }
}

unsigned int cheat_file_size;
extern unsigned char CheatOn;
void DisableCheatsOnLoad(), EnableCheatsOnLoad(), ConvertCheatFileFormat();
extern unsigned int GUIcurrentcheatcursloc;

void CheatCodeLoad()
{
  FILE *fp = 0;

  setextension(ZSaveName, "cht");
  GUICBHold = 0;

  if ((fp = fopen_dir(ZSramPath,ZSaveName,"rb")))
  {
    asm_call(DisableCheatsOnLoad);

    cheat_file_size = fread(cheatdata, 1, 255*28, fp);
    fclose(fp);

    if(cheatdata[6]==254 && cheatdata[7]==252)
      NumCheats = cheat_file_size / 28;
    else asm_call(ConvertCheatFileFormat);

    asm_call(EnableCheatsOnLoad);

    if (NumCheats <= GUIcurrentcheatcursloc) GUIcurrentcheatcursloc=NumCheats-1;
    if (NumCheats) CheatOn=1;
    else GUIcurrentcheatcursloc=CheatOn=0;
  }
}

extern unsigned char *vidbuffer;

void SaveCheatSearchFile()
{
  FILE *fp = 0;

  if ((fp = fopen_dir(ZCfgPath,"tmpchtsr.___","wb")))
  {
    fwrite(vidbuffer+129600, 1, 65536*2+32768, fp);
    fclose(fp);
  }
}

void LoadCheatSearchFile()
{
  FILE *fp = 0;

  if ((fp = fopen_dir(ZCfgPath,"tmpchtsr.___","rb")))
  {
    fread(vidbuffer+129600, 1, 65536*2+32768, fp);
    fclose(fp);
  }
}

#define HEADER_SIZE 512
#define INFO_LEN (0xFF - 0xC0)

int InfoScore(char *);
unsigned int sum(unsigned char *array, unsigned int size);

static void get_rom_name(const char *filename, char *namebuffer)
{
  char *last_dot = strrchr(filename, '.');
  if (!last_dot || (strcasecmp(last_dot, ".zip") && strcasecmp(last_dot, ".gz") && strcasecmp(last_dot, ".jma")))
  {
    struct stat filestats;
    stat(filename, &filestats);

    if ((filestats.st_size >= 0x8000) && (filestats.st_size <= 0x600000))
    {
      FILE *fp = fopen(filename, "rb");
      if (fp)
      {
        unsigned char HeaderBuffer[HEADER_SIZE];
        int HeaderSize = 0, HasHeadScore = 0, NoHeadScore = 0, HeadRemain = filestats.st_size & 0x7FFF;
        bool EHi = false;

        switch(HeadRemain)
        {
          case 0:
            NoHeadScore += 3;
            break;

          case HEADER_SIZE:
            HasHeadScore += 2;
            break;
        }

        fread(HeaderBuffer, 1, HEADER_SIZE, fp);

        if (sum(HeaderBuffer, HEADER_SIZE) < 2500) { HasHeadScore += 2; }

        //SMC/SWC Header
        if (HeaderBuffer[8] == 0xAA && HeaderBuffer[9] == 0xBB && HeaderBuffer[10]== 4)
        {
          HasHeadScore += 3;
        }
        //FIG Header
        else if ((HeaderBuffer[4] == 0x77 && HeaderBuffer[5] == 0x83) ||
                 (HeaderBuffer[4] == 0xDD && HeaderBuffer[5] == 0x82) ||
                 (HeaderBuffer[4] == 0xDD && HeaderBuffer[5] == 2) ||
                 (HeaderBuffer[4] == 0xF7 && HeaderBuffer[5] == 0x83) ||
                 (HeaderBuffer[4] == 0xFD && HeaderBuffer[5] == 0x82) ||
                 (HeaderBuffer[4] == 0x00 && HeaderBuffer[5] == 0x80) ||
                 (HeaderBuffer[4] == 0x47 && HeaderBuffer[5] == 0x83) ||
                 (HeaderBuffer[4] == 0x11 && HeaderBuffer[5] == 2))
        {
          HasHeadScore += 2;
        }
        else if (!strncmp("GAME DOCTOR SF 3", (char *)HeaderBuffer, 16))
        {
          HasHeadScore += 5;
        }

        HeaderSize = HasHeadScore > NoHeadScore ? HEADER_SIZE : 0;

        if (filestats.st_size - HeaderSize >= 0x500000)
        {
          fseek(fp, 0x40FFC0 + HeaderSize, SEEK_SET);
          fread(HeaderBuffer, 1, INFO_LEN, fp);
          if (InfoScore((char *)HeaderBuffer) > 1)
          {
            EHi = true;
            strncpy(namebuffer, (char *)HeaderBuffer, 21);
          }
        }

        if (!EHi)
        {
          if (filestats.st_size - HeaderSize >= 0x10000)
          {
            char LoHead[INFO_LEN], HiHead[INFO_LEN];
            int LoScore, HiScore;

            fseek(fp, 0x7FC0 + HeaderSize, SEEK_SET);
            fread(LoHead, 1, INFO_LEN, fp);
            LoScore = InfoScore(LoHead);

            fseek(fp, 0xFFC0 + HeaderSize, SEEK_SET);
            fread(HiHead, 1, INFO_LEN, fp);
            HiScore = InfoScore(HiHead);

            strncpy(namebuffer, LoScore > HiScore ? LoHead : HiHead, 21);

            if (filestats.st_size - HeaderSize >= 0x20000)
            {
              int IntLScore;
              fseek(fp, (filestats.st_size - HeaderSize) / 2 + 0x7FC0 + HeaderSize, SEEK_SET);
              fread(LoHead, 1, INFO_LEN, fp);
              IntLScore = InfoScore(LoHead) / 2;

              if (IntLScore > LoScore && IntLScore > HiScore)
              {
                strncpy(namebuffer, LoHead, 21);
              }
            }
          }
          else //ROM only has one block
          {
            fseek(fp, 0x7FC0 + HeaderSize, SEEK_SET);
            fread(namebuffer, 21, 1, fp);
          }
        }
        fclose(fp);
      }
      else //Couldn't open file
      {
        strcpy(namebuffer, "** READ FAILURE **");
      }
    }
    else //Smaller than a block, or Larger than 6MB
    {
      strcpy(namebuffer, "** INVALID FILE **");
    }
  }
  else //Compressed archive
  {
    strcpy(namebuffer, filename);
  }
}

extern unsigned char spcRamcmp[65536];
extern unsigned int GUInumentries;
extern unsigned char *spcBuffera;

void GetLoadHeader()
{
  unsigned int i = 0;
  while (i < GUInumentries)
  {
    get_rom_name((char *)(spcRamcmp+1+i*14), (char *)(spcBuffera+1+i*32));
    i++;
  }
}

void dumpsound()
{
  FILE *fp = fopen_dir(ZSpcPath, "sounddmp.raw", "wb");
  if (fp)
  {
    fwrite(spcBuffera, 1, 65536*4+4096, fp);
    fclose(fp);
  }
}

extern unsigned char GUIwinorder[18], GUIwinactiv[18], pressed[448];
extern unsigned int GUIindex;

static void memswap(void *p1, void *p2, size_t p2len)
{
  char *ptr1 = (char *)p1;
  char *ptr2 = (char *)p2;

  size_t p1len = ptr2 - ptr1;
  unsigned char byte;
  while (p2len--)
  {
    byte = *ptr2++;
    memmove(ptr1+1, ptr1, p1len);
    *ptr1++ = byte;
  }
}

void GUIloadfilename(), GUIQuickLoadUpdate();

void loadquickfname()
{
  memset(GUIwinorder, 0, sizeof(GUIwinorder));
  memset(GUIwinactiv, 0, sizeof(GUIwinactiv));

  if (!access(ZRomPath,R_OK))
  {
    //Move menuitem to top
    if (GUIindex || !prevlfreeze)
    {
      memswap(prevloadnames,prevloadnames+GUIindex*16,16);
      memswap(prevloadfnamel,prevloadfnamel+GUIindex*512,512);
      memswap(prevloaddnamel,prevloaddnamel+GUIindex*512,512);

      strcpy(ZRomPath, (char *)prevloaddnamel);
      strcpy(ZCartName, (char *)prevloadfnamel);

      //Clear pressed
      memset(pressed, 0, sizeof(pressed));

      asm_call(GUIQuickLoadUpdate);
    }
    asm_call(GUIloadfilename);
  }
}

char *gui_rom;
void init_rom_path_gui()
{
  init_rom_path(gui_rom);
}