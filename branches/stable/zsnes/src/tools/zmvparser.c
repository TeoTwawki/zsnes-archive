/*
Copyright (C) 2005 Nach, grinvader

Heavily based on zsnes/src/zmovie.c revision 1.95
*/

#define EZP_VERSION "1.24"

#ifdef __LINUX__
#include "gblhdr.h"
#define DIR_SLASH "/"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#define DIR_SLASH "\\"
#endif

#ifndef NUMCONV_H
#define NUMCONV_H

// Get correct mask for particular bit
#define BIT(X) (1 << X)

/*
Functions that the compiler should inline that will convert
uint32, uint24, uint16 to 4 byte, 3 byte, 2 byte arrays. -Nach
*/

static unsigned int bytes_to_uint32(const unsigned char buffer[4])
{
  unsigned int num = (unsigned int)buffer[0];
  num |= ((unsigned int)buffer[1]) << 8;
  num |= ((unsigned int)buffer[2]) << 16;
  num |= ((unsigned int)buffer[3]) << 24;
  return(num);
}

static unsigned int bytes_to_uint24(const unsigned char buffer[3])
{
  unsigned int num = (unsigned int)buffer[0];
  num |= ((unsigned int)buffer[1]) << 8;
  num |= ((unsigned int)buffer[2]) << 16;
  return(num);
}

static unsigned short bytes_to_uint16(const unsigned char buffer[2])
{
  unsigned short num = (unsigned short)buffer[0];
  num |= ((unsigned short)buffer[1]) << 8;
  return(num);
}

// Functions to read 2, 3, 4 bytes and convert to uint16, uint24, uint32
static unsigned short fread2(FILE *fp)
{
  unsigned char uint16buf[2];
  fread(uint16buf, 2, 1, fp);
  return(bytes_to_uint16(uint16buf));
}

static unsigned int fread3(FILE *fp)
{
  unsigned char uint24buf[3];
  fread(uint24buf, 3, 1, fp);
  return(bytes_to_uint24(uint24buf));
}

static unsigned int fread4(FILE *fp)
{
  unsigned char uint32buf[4];
  fread(uint32buf, 4, 1, fp);
  return(bytes_to_uint32(uint32buf));
}

#endif

/////////////////////////////////////////////////////////

/*
ZMV Format

-----------------------------------------------------------------
Header
-----------------------------------------------------------------

3 bytes  -  "ZMV"
2 bytes  -  ZMV Version # (version of ZSNES)
4 bytes  -  CRC32 of ROM
4 bytes  -  Number of frames in this movie
4 bytes  -  Number of rerecords
4 bytes  -  Number of frames removed by rerecord
4 bytes  -  Number of frames advanced step-by-step
1 byte   -  Average recording FPS (includes dropped frames) x4-5 for precision
4 bytes  -  Number of key combos
2 bytes  -  Number of internal chapters
2 bytes  -  Length of author name
3 bytes  -  Uncompressed ZST size
2 bytes  -  Initial input configuration
  1 bit  -   Input 1 enabled
  1 bit  -   Input 2 enabled
  1 bit  -   Input 3 enabled
  1 bit  -   Input 4 enabled
  1 bit  -   Input 5 enabled
  1 bit  -   Mouse in first port
  1 bit  -   Mouse in second port
  1 bit  -   Super Scope in second port
  8 bits -   Reserved
1 byte   -  Flag Byte
  2 bits -   Start from ZST/Power On/Reset/Power On + Clear SRAM
  1 bit  -   NTSC or PAL
  5 bits -   Reserved
3 bytes  -  1 bit for compressed or not, 23 bits for size
ZST size -  ZST (no thumbnail)


-----------------------------------------------------------------
Key input  -  Repeated for all input / internal chapters
-----------------------------------------------------------------

1 byte   - Flag Byte
  1 bit  -  Controller 1 changed
  1 bit  -  Controller 2 changed
  1 bit  -  Controller 3 changed
  1 bit  -  Controller 4 changed
  1 bit  -  Controller 5 changed
  1 bit  -  Chapter instead of input here
  1 bit  -  RLE instead of input
  1 bit  -  Command here

-If Command-
Remaining 7 bits of flag determine command

-Else If RLE-
4 bytes  -  Frame # to repeat previous input to

-Else If Chapter-

3 bytes  -  1 bit for compressed or not, 23 bits for size
ZST size -  ZST
4 bytes  -  Frame #
2 bytes  -  Controller Status
9 bytes  -  Maximum previous input (1 Mouse [18] + 4 Regular [12*4] + 6 padded bits)

-Else-

variable - Input

  12 bits per regular controller, 18 per mouse where input changed padded to next full byte size
  Minimum 2 bytes  (12 controller bits + 4 padded bits)
  Maximum 9 bytes (18 mouse controller bits + 48 regular controller bits [12*4] + 6 padded bits)


-----------------------------------------------------------------
Internal chapter offsets  -  Repeated for all internal chapters
-----------------------------------------------------------------

4 bytes  -  Offset to chapter from beginning of file (after input flag byte for ZST)


-----------------------------------------------------------------
External chapters  -  Repeated for all external chapters
-----------------------------------------------------------------

ZST Size -  ZST (never compressed)
4 bytes  -  Frame #
2 bytes  -  Controller Status
9 bytes  -  Maximum previous input (1 Mouse [18] + 4 Regular [12*4] + 6 padded bits)
4 bytes  -  Offset to input for current chapter from beginning of file


-----------------------------------------------------------------
External chapter count
-----------------------------------------------------------------

2 bytes  - Number of external chapters

-----------------------------------------------------------------
Author name
-----------------------------------------------------------------

Name Len - Author's name

*/


/*

ZMV header types, vars, and functions

*/

enum zmv_start_methods { zmv_sm_zst, zmv_sm_power, zmv_sm_reset, zmv_sm_clear_all };
enum zmv_video_modes { zmv_vm_ntsc, zmv_vm_pal };

#define INT_CHAP_SIZE(offset) (internal_chapter_length(offset)+4+2+9)
#define EXT_CHAP_SIZE (zmv_vars.header.zst_size+4+2+9+4)

#define INT_CHAP_INDEX_SIZE (zmv_vars.header.internal_chapters*4)
#define EXT_CHAP_BLOCK_SIZE (zmv_open_vars.external_chapter_count*EXT_CHAP_SIZE + 2)

#define EXT_CHAP_END_DIST (EXT_CHAP_BLOCK_SIZE + (size_t)zmv_vars.header.author_len)
#define INT_CHAP_END_DIST (INT_CHAP_INDEX_SIZE + EXT_CHAP_END_DIST)

#define EXT_CHAP_COUNT_END_DIST ((size_t)zmv_vars.header.author_len + 2)

struct zmv_header
{
  char magic[3];
  unsigned short zsnes_version;
  unsigned int rom_crc32;
  unsigned int frames;
  unsigned int rerecords;
  unsigned int removed_frames;
  unsigned int incr_frames;
  unsigned char average_fps;
  unsigned int key_combos;
  unsigned short internal_chapters;
  unsigned short author_len;
  unsigned int zst_size; //We only read/write 3 bytes for this
  unsigned short initial_input;
  struct
  {
    enum zmv_start_methods start_method;
    enum zmv_video_modes video_mode;
  } zmv_flag;
};

#define AUTHOR_LEN_LOC 32
unsigned char flag;

static bool zmv_header_read(struct zmv_header *zmv_head, FILE *fp)
{
  fread(zmv_head->magic, 3, 1, fp);
  zmv_head->zsnes_version = fread2(fp);
  zmv_head->rom_crc32 = fread4(fp);
  zmv_head->frames = fread4(fp);
  zmv_head->rerecords = fread4(fp);
  zmv_head->removed_frames = fread4(fp);
  zmv_head->incr_frames = fread4(fp);
  fread(&zmv_head->average_fps, 1, 1, fp);
  zmv_head->key_combos = fread4(fp);
  zmv_head->internal_chapters = fread2(fp);
  zmv_head->author_len = fread2(fp);
  zmv_head->zst_size = fread3(fp);
  zmv_head->initial_input = fread2(fp);
  fread(&flag, 1, 1, fp);

  if (feof(fp))
  {
    return(false);
  }

  switch (flag & (BIT(7)|BIT(6)))
  {
    case 0:
      zmv_head->zmv_flag.start_method = zmv_sm_zst;
      break;

    case BIT(7):
      zmv_head->zmv_flag.start_method = zmv_sm_power;
      break;

    case BIT(6):
      zmv_head->zmv_flag.start_method = zmv_sm_reset;
      break;

    case BIT(7)|BIT(6):
      zmv_head->zmv_flag.start_method = zmv_sm_clear_all;
      break;
  }

  switch (flag & BIT(5))
  {
    case 0:
      zmv_head->zmv_flag.video_mode = zmv_vm_ntsc;
      break;

    case BIT(5):
      zmv_head->zmv_flag.video_mode = zmv_vm_pal;
      break;
  }

  if (flag & (BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0)))
  {
    return(false);
  }

  return(true);
}

/*

Internal chapter types, vars, and functions

*/

#define INTERNAL_CHAPTER_BUF_LIM 10
struct internal_chapter_buf
{
  size_t offsets[INTERNAL_CHAPTER_BUF_LIM];
  unsigned char used;
  struct internal_chapter_buf *next;
};

static void internal_chapter_add_offset(struct internal_chapter_buf *icb, size_t offset)
{
  while (icb->next)
  {
    icb = icb->next;
  }

  if (icb->used == INTERNAL_CHAPTER_BUF_LIM)
  {
    icb->next = (struct internal_chapter_buf *)malloc(sizeof(struct internal_chapter_buf));
    icb = icb->next;
    memset(icb, 0, sizeof(struct internal_chapter_buf));
  }

  icb->offsets[icb->used] = offset;
  icb->used++;
}

static void internal_chapter_free_chain(struct internal_chapter_buf *icb)
{
  if (icb)
  {
    if (icb->next)
    {
      internal_chapter_free_chain(icb->next);
    }
    free(icb);
  }
}

static void internal_chapter_read(struct internal_chapter_buf *icb, FILE *fp, size_t count)
{
  while (count--)
  {
    internal_chapter_add_offset(icb, fread4(fp));
  }
}

size_t internal_chapter_pos(struct internal_chapter_buf *icb, size_t offset)
{
  size_t pos = 0;
  do
  {
    unsigned char i;
    for (i = 0; i < icb->used; i++, pos++)
    {
      if (icb->offsets[i] == offset)
      {
        return(pos);
      }
    }
  } while ((icb = icb->next));
  return(~0);
}

static size_t chapter_greater(struct internal_chapter_buf *icb, size_t offset)
{
  size_t greater = ~0;
  do
  {
    unsigned char i;
    for (i = 0; i < icb->used; i++)
    {
      if ((icb->offsets[i] > offset) && (icb->offsets[i] < greater))
      {
        greater = icb->offsets[i];
      }
    }
  } while ((icb = icb->next));
  return((greater == ~0) ? offset : greater);
}

/*

Bit Decoder

*/

/*
When working with bits, you have to find the bits in a byte.

Divide the amount of bits by 8 (bit_count >> 3) to find the proper byte.
The proper bit number in the byte is the amount of bits modulo 8 (bit_count&7).
To get the most significant bit, you want the bit which is 7 minus the proper
bit number.
*/

size_t bit_decoder(unsigned int *data, unsigned int mask, unsigned char *buffer, size_t skip_bits)
{
  unsigned char bit_loop;
  *data = 0;

  for (bit_loop = 31; ; bit_loop--)
  {
    if (mask & BIT(bit_loop))
    {
      if (buffer[skip_bits>>3] & BIT((7-(skip_bits&7))))
      {
        *data |= BIT(bit_loop);
      }
      skip_bits++;
    }

    if (!bit_loop) { break; }
  }

  return(skip_bits);
}

/*

Shared var between record/replay functions

*/

#define WRITE_BUFFER_SIZE 1024
static struct
{
  struct zmv_header header;
  FILE *fp;
  struct
  {
    unsigned int A;
    unsigned int B;
    unsigned int C;
    unsigned int D;
    unsigned int E;
    unsigned short latchx;
    unsigned short latchy;
  } last_joy_state;
  unsigned short inputs_enabled;
  unsigned char write_buffer[WRITE_BUFFER_SIZE];
  size_t write_buffer_loc;
  struct internal_chapter_buf internal_chapters;
  size_t last_internal_chapter_offset;
  char *filename;
  size_t rle_count;
} zmv_vars;

#define GAMEPAD_MASK 0xFFF00000
#define MOUSE_MASK 0x00C0FFFF
#define SCOPE_MASK 0xF0000000

#define GAMEPAD_ENABLE 0x00008000
#define MOUSE_ENABLE 0x00010000
#define SCOPE_ENABLE 0x00FF0000

static size_t pad_bit_decoder(unsigned char pad, unsigned char *buffer, size_t skip_bits)
{
  unsigned int *last_state = 0;
  unsigned short input_enable_mask = 0;

  switch (pad)
  {
    case 1:
      last_state = &zmv_vars.last_joy_state.A;
      input_enable_mask = BIT(0xF);
      break;

    case 2:
      last_state = &zmv_vars.last_joy_state.B;
      input_enable_mask = BIT(0xE);
      break;

    case 3:
      last_state = &zmv_vars.last_joy_state.C;
      input_enable_mask = BIT(0xD);
      break;

    case 4:
      last_state = &zmv_vars.last_joy_state.D;
      input_enable_mask = BIT(0xC);
      break;

    case 5:
      last_state = &zmv_vars.last_joy_state.E;
      input_enable_mask = BIT(0xB);
      break;
  }

  switch (pad)
  {
    case 2:
      if (BIT(0x8)) //Super Scope
      {
        unsigned int xdata, ydata;

        skip_bits = bit_decoder(last_state, SCOPE_MASK, buffer, skip_bits);
        skip_bits = bit_decoder(&xdata, 0x000000FF, buffer, skip_bits);
        skip_bits = bit_decoder(&ydata, 0x000000FF, buffer, skip_bits);
        *last_state |= SCOPE_ENABLE;

        zmv_vars.last_joy_state.latchx = (unsigned short)(xdata + 40);
        zmv_vars.last_joy_state.latchy = (unsigned short)ydata;

        break;
      }

    case 1:
      if (zmv_vars.inputs_enabled & ((pad == 1) ? BIT(0xA) : BIT(0x9))) //Mouse ?
      {
        skip_bits = bit_decoder(last_state, MOUSE_MASK, buffer, skip_bits);
        *last_state |= MOUSE_ENABLE;
      }
      else
      {
        skip_bits = bit_decoder(last_state, GAMEPAD_MASK, buffer, skip_bits);
        *last_state |= (zmv_vars.inputs_enabled & input_enable_mask) ? GAMEPAD_ENABLE : 0;
      }
      break;

    case 3: case 4: case 5:
      //No multitap if both ports use special devices
      if ((zmv_vars.inputs_enabled & (BIT(0xA)|BIT(0x9)|BIT(0x8))) > BIT(0xA))
      {
        *last_state = 0;
      }
      else
      {
        skip_bits = bit_decoder(last_state, GAMEPAD_MASK, buffer, skip_bits);
        *last_state |= (zmv_vars.inputs_enabled & input_enable_mask) ? GAMEPAD_ENABLE : 0;
      }
      break;
  }
  return(skip_bits);
}

// For various ZMV calculations, the length of the last chapter needs to be known
static size_t internal_chapter_length(size_t offset)
{
  size_t current_loc = ftell(zmv_vars.fp);
  size_t icl = 0;

  fseek(zmv_vars.fp, offset, SEEK_SET);
  icl = fread3(zmv_vars.fp) & 0x007FFFFF; // The upper 9 bits are not part of the length
  icl += 3; // Add 3 for the header which says how long it is

  fseek(zmv_vars.fp, current_loc, SEEK_SET);
  return(icl);
}

/*

Open and replay ZMV

*/

typedef struct internal_chapter_buf external_chapter_buf;

static struct
{
  external_chapter_buf external_chapters;
  unsigned short external_chapter_count;
  unsigned int frames_replayed;
  size_t last_chapter_frame;
  size_t first_chapter_pos;
  size_t input_start_pos;
} zmv_open_vars; // Additional vars for open/replay of a ZMV

static bool zmv_open(char *filename)
{
  memset(&zmv_vars, 0, sizeof(zmv_vars));
  memset(&zmv_open_vars, 0, sizeof(zmv_open_vars));

  zmv_vars.fp = fopen(filename,"r+b");
  if (zmv_vars.fp && zmv_header_read(&zmv_vars.header, zmv_vars.fp) &&
      !strncmp(zmv_vars.header.magic, "ZMV", 3))
  {
    unsigned short i;
    size_t filename_len = strlen(filename);

    zmv_vars.inputs_enabled = zmv_vars.header.initial_input;
    zmv_open_vars.first_chapter_pos = ftell(zmv_vars.fp);

    fseek(zmv_vars.fp, internal_chapter_length(ftell(zmv_vars.fp)), SEEK_CUR); // dummy state
    zmv_open_vars.input_start_pos = ftell(zmv_vars.fp);

    fseek(zmv_vars.fp, -(EXT_CHAP_COUNT_END_DIST), SEEK_END);
    zmv_open_vars.external_chapter_count = fread2(zmv_vars.fp);

    fseek(zmv_vars.fp, -(INT_CHAP_END_DIST), SEEK_END);

    internal_chapter_read(&zmv_vars.internal_chapters, zmv_vars.fp, zmv_vars.header.internal_chapters);

    for (i = 0; i < zmv_open_vars.external_chapter_count; i++)
    {
// Seek to 4 bytes before end of chapter, since last 4 bytes is where it contains offset value
      fseek(zmv_vars.fp, EXT_CHAP_SIZE-4, SEEK_CUR);
      internal_chapter_add_offset(&zmv_open_vars.external_chapters, fread4(zmv_vars.fp));
    }

    fseek(zmv_vars.fp, zmv_open_vars.input_start_pos, SEEK_SET);

    zmv_vars.filename = (char *)malloc(filename_len+1); // +1 for null
    strcpy(zmv_vars.filename, filename);

    return(true);
  }
  return(false);
}

static void replay_pad(unsigned char pad, unsigned char flag, unsigned char *buffer, size_t *skip_bits)
{
  unsigned int *last_state = 0;
  unsigned char bit_mask = 0;

  switch (pad)
  {
    case 1:
      last_state = &zmv_vars.last_joy_state.A;
      bit_mask = BIT(7);
      break;
    case 2:
      last_state = &zmv_vars.last_joy_state.B;
      bit_mask = BIT(6);
      break;
    case 3:
      last_state = &zmv_vars.last_joy_state.C;
      bit_mask = BIT(5);
      break;
    case 4:
      last_state = &zmv_vars.last_joy_state.D;
      bit_mask = BIT(4);
      break;
    case 5:
      last_state = &zmv_vars.last_joy_state.E;
      bit_mask = BIT(3);
      break;
  }

  if (flag & bit_mask)
  {
    size_t bits_needed = pad_bit_decoder(pad, buffer, 0);
    if (bits_needed)
    {
      size_t leftover_bits = (8 - (*skip_bits&7)) & 7;
      bits_needed -= leftover_bits;

      fread(buffer + (*skip_bits>>3) + ((*skip_bits&7) ? 1 : 0), 1, (bits_needed>>3) + ((bits_needed&7) ? 1 : 0), zmv_vars.fp);
      *skip_bits = pad_bit_decoder(pad, buffer, *skip_bits);
    }
  }
}

/*
static bool zmv_replay()
{
  if (zmv_open_vars.frames_replayed < zmv_vars.header.frames)
  {
    if (zmv_vars.rle_count) { zmv_vars.rle_count--; }
    else
    {
      zmv_vars.rle_count = 0;

      fread(&flag, 1, 1, zmv_vars.fp);

      if (flag & BIT(0))	{ return(zmv_replay()); }

      else if (flag & BIT(1)) // RLE
      {
        zmv_vars.rle_count = fread4(zmv_vars.fp) - zmv_open_vars.frames_replayed;
        return(zmv_replay());
      }

      else if (flag & BIT(2)) // Internal Chapter
      {
        fseek(zmv_vars.fp, INT_CHAP_SIZE(ftell(zmv_vars.fp)), SEEK_CUR);
        return(zmv_replay());
      }

      else
      {
        unsigned char press_buf[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        size_t skip_bits = 0;

        replay_pad(1, flag, press_buf, &skip_bits);
        replay_pad(2, flag, press_buf, &skip_bits);
        replay_pad(3, flag, press_buf, &skip_bits);
        replay_pad(4, flag, press_buf, &skip_bits);
        replay_pad(5, flag, press_buf, &skip_bits);
      }
    }

    zmv_open_vars.frames_replayed++;
    return(true);
  }

  return(false);
}
*/

static bool zmv_replay()
{
  if (zmv_open_vars.frames_replayed < zmv_vars.header.frames)
  {
    if (zmv_vars.rle_count) { zmv_vars.rle_count--; }
    else
    {
      zmv_vars.rle_count = 0;

      fread(&flag, 1, 1, zmv_vars.fp);

      if (flag & BIT(0)) //Command
      {
        return(zmv_replay());
      }

      else if (flag & BIT(1)) //RLE
      {
        zmv_vars.rle_count = fread4(zmv_vars.fp) - zmv_open_vars.frames_replayed;
        return(zmv_replay());
      }

      else if (flag & BIT(2)) //Internal Chapter
      {
        fseek(zmv_vars.fp, INT_CHAP_SIZE(ftell(zmv_vars.fp)), SEEK_CUR);
        return(zmv_replay());
      }

      else
      {
       unsigned char press_buf[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        size_t skip_bits = 0;

        replay_pad(1, flag, press_buf, &skip_bits);
        replay_pad(2, flag, press_buf, &skip_bits);
        replay_pad(3, flag, press_buf, &skip_bits);
        replay_pad(4, flag, press_buf, &skip_bits);
        replay_pad(5, flag, press_buf, &skip_bits);
      }
    }

    zmv_open_vars.frames_replayed++;
    return(true);
  }

  return(false);
}

unsigned char *chapter_type;
size_t chapter_offset;

static bool zmv_next_chapter()
{
  size_t current_loc = ftell(zmv_vars.fp);

  size_t next_internal = chapter_greater(&zmv_vars.internal_chapters, current_loc);
  size_t next_external = chapter_greater(&zmv_open_vars.external_chapters, current_loc);

  size_t next = 0;

  if (next_internal != current_loc)
  {
    next = next_internal;
  }
  else
  {
    next_internal = ~0;
  }

  if ((next_external != current_loc) && next_external < next_internal)
  {
    next = next_external;
  }

  if (next)
  {
    if (next == next_internal)
    {
      fseek(zmv_vars.fp, next_internal, SEEK_SET);
      chapter_type = "Internal";
      chapter_offset = next;
      fseek(zmv_vars.fp, internal_chapter_length(ftell(zmv_vars.fp)), SEEK_CUR); // dummy state
      zmv_open_vars.frames_replayed = fread4(zmv_vars.fp);
      fseek(zmv_vars.fp, 11, SEEK_CUR); // dummy controller status
    }
    else
    {
      size_t ext_chapter_loc = EXT_CHAP_END_DIST - internal_chapter_pos(&zmv_open_vars.external_chapters, next)*EXT_CHAP_SIZE;
      fseek(zmv_vars.fp, -(ext_chapter_loc), SEEK_END);
      chapter_type = "External";
      chapter_offset = ftell(zmv_vars.fp);
      fseek(zmv_vars.fp, zmv_vars.header.zst_size, SEEK_CUR); // dummy state
      zmv_open_vars.frames_replayed = fread4(zmv_vars.fp);
	// no dummy input since next line takes care of file pointer location
      fseek(zmv_vars.fp, next_external, SEEK_SET);
    }

    zmv_vars.rle_count = 0;
    return(true);
  }

  return(false);
}

static void zmv_replay_finished()
{
  internal_chapter_free_chain(zmv_vars.internal_chapters.next);
  internal_chapter_free_chain(zmv_open_vars.external_chapters.next);
  free(zmv_vars.filename);
  fclose(zmv_vars.fp);
}

/*

Parser functions

*/

static void display_author()
{
  if (zmv_vars.header.author_len)
  {
    unsigned char *author = (unsigned char *)malloc(zmv_vars.header.author_len+1); // +1 for null

    fseek(zmv_vars.fp, -(zmv_vars.header.author_len), SEEK_END);
    fread(author, 1, zmv_vars.header.author_len, zmv_vars.fp);
    author[zmv_vars.header.author_len] = 0;
    printf("Author name: %s\n", author);
    free(author);
  }
  else  { puts("Anonymous movie."); }
}

static void basic_info()
{
  unsigned int duration,weeks,days,hours,minutes;
  float seconds = 0.0;
  unsigned char *buffer;

  printf("ZMV Version: %u\n", zmv_vars.header.zsnes_version);
  printf("ROM CRC32: %08X\n", zmv_vars.header.rom_crc32);
  duration = zmv_vars.header.frames;

  switch (zmv_vars.header.zmv_flag.video_mode)
  {
    case zmv_vm_ntsc:
      buffer = "NTSC (60FPS)";
      seconds = ((float)duration)/60.0f;
      duration /= 60;
      break;
    case zmv_vm_pal:
      buffer = "PAL (50FPS)";
      seconds = ((float)duration)/50.0f;
      duration /= 50;
      break;
    default:
      buffer = "invalid";
      duration = 0;
  }

  printf("Movie timing is %s.\n", buffer);
  printf("Uncompressed ZST size: %u\n", zmv_vars.header.zst_size);

  switch (zmv_vars.header.zmv_flag.start_method)
  {
    case zmv_sm_zst:
      buffer = "s from ZST";
      break;
    case zmv_sm_reset:
      buffer = "s from reset";
      break;
    case zmv_sm_power:
      buffer = "s from power-on";
      break;
    case zmv_sm_clear_all:
      buffer = "s from power-on with clear SRAM";
      break;
    default:
      buffer = " method invalid";
  }

  printf("ZMV start%s.\n\n", buffer);

  display_author();
  printf("Total frames: %u\n", zmv_vars.header.frames);

  if (duration) // to prevent div by 0 errors, and to show only useful info
  {
    weeks = duration / 604800;
    days = duration / 86400 - 7*weeks;
    hours = duration / 3600 - 7*24*weeks - 24*days;
    minutes = duration / 60 - 7*24*60*weeks - 24*60*days - 60*hours;
    seconds = seconds - 7*24*3600*weeks - 24*3600*days - 3600*hours - 60*minutes;
    printf("Estimated duration: ");
    if (weeks)                              { printf("%uw ", weeks); }
    if (weeks || days)                      { printf("%ud ", days); }
    if (weeks || days || hours)             { printf("%0*u:", (weeks || days) ? 2 : 1, hours); }
    if (weeks || days || hours || minutes)  { printf("%0*u:", (weeks || days || hours) ? 2 : 1, minutes); }
    printf("%0*.3f", (weeks || days || hours || minutes) ? 6 : 5, seconds);
    if (!weeks && !days && !hours && !minutes)  { printf(" seconds"); }
    puts("");

    printf("Re-recorded %u times.\n", zmv_vars.header.rerecords);
    printf("Frames dropped by re-recordings: %u\n", zmv_vars.header.removed_frames);
    printf("Frames advanced step-by-step: %u (%u%%)\n", zmv_vars.header.incr_frames
	, zmv_vars.header.incr_frames*100/(zmv_vars.header.frames + zmv_vars.header.removed_frames));
    printf("Average recording FPS: %.2f\n", ((float)zmv_vars.header.average_fps)/((zmv_vars.header.zmv_flag.video_mode == zmv_vm_ntsc) ? 4.0f : 5.0f));
    printf("Key combinations used %u times.\n\n", zmv_vars.header.key_combos);

    printf("%u internal chapters.\n", zmv_vars.header.internal_chapters);
    printf("%u external chapters.\n\n", zmv_open_vars.external_chapter_count);
  }
  else  { puts("State-only movie file.\n"); }
}

static void chapter_info()
{
  if ((zmv_vars.header.internal_chapters + zmv_open_vars.external_chapter_count))
  {
    puts("Detailed chapter info:");

    while (zmv_next_chapter())
    {
      printf("%s chapter at offset 0x%X: next frame is #%u at offset 0x%lX.\n"
	, chapter_type, chapter_offset, zmv_open_vars.frames_replayed + 1
	, ftell(zmv_vars.fp));
    }

    puts("");
  }
  else  { puts("No chapters.\n"); }
}

/*static void dechapterizator()
{
  if ((zmv_vars.header.internal_chapters + zmv_open_vars.external_chapter_count))
  {
    while (zmv_next_chapter())
    {
      printf("%s chapter at offset 0x%X - Delete (yea/nay) ?\n", chapter_type, chapter_offset);

    }

    printf("\nDeletion result - ");

  }
  else
  {
    printf("Deletion cancelled - ");
  }
}*/

unsigned char decoded[44] = "Up Down Left Right Start Select A B X Y L R";
/*                           0123456789012345678901234567890123456789012
                             0         1         2         3         4
12 bits encoded input
<--- byte 1 ---->    <----------- byte 0 ------------>
3  2    1       0    7    6     5      4    3  2  1  0
B, Y, Select, Start, Up, Down, Left, Right, A, X, L, R
*/

static unsigned char *decode_input(unsigned int pad_status)
{
  memset(decoded, ' ', 43);
  pad_status >>= 20;

  if (pad_status & 0x001)	{ memcpy(decoded+42, "R", 1); }
  if (pad_status & 0x002)	{ memcpy(decoded+40, "L", 1); }
  if (pad_status & 0x004)	{ memcpy(decoded+36, "X", 1); }
  if (pad_status & 0x008)	{ memcpy(decoded+32, "A", 1); }
  if (pad_status & 0x010)	{ memcpy(decoded+13, "Right", 5); }
  if (pad_status & 0x020)	{ memcpy(decoded+8,  "Left", 4); }
  if (pad_status & 0x040)	{ memcpy(decoded+3,  "Down", 4); }
  if (pad_status & 0x080)	{ memcpy(decoded+0,  "Up", 2); }
  if (pad_status & 0x100)	{ memcpy(decoded+19, "Start", 5); }
  if (pad_status & 0x200)	{ memcpy(decoded+25, "Select", 6); }
  if (pad_status & 0x400)	{ memcpy(decoded+38, "Y", 1); }
  if (pad_status & 0x800)	{ memcpy(decoded+34, "B", 1); }

  return (decoded);
}

static void input_info(const unsigned int spec_frame, const bool full_dump)
{
  bool task_done = false;

  if (spec_frame)
  {
    if (spec_frame > zmv_vars.header.frames)
    {
      printf("#%u is over the total frames in this movie.\n\n", spec_frame);
    }
    else
    {
      printf("Searching for frame %u... please wait\n", spec_frame);
    }
  }

  zmv_open_vars.frames_replayed = 0;
  zmv_vars.last_joy_state.A = 0;
  zmv_vars.last_joy_state.B = 0;
  zmv_vars.last_joy_state.C = 0;
  zmv_vars.last_joy_state.D = 0;
  zmv_vars.last_joy_state.E = 0;

  if (spec_frame <= zmv_vars.header.frames)
  {
    while (!task_done)
    {
      if (!zmv_replay())
      {
        if (!spec_frame)	{ puts("End of input.\n"); }
        task_done = true;
      }
      else
      {
        if (full_dump)
        {
          if ((!spec_frame) || (spec_frame == zmv_open_vars.frames_replayed))
          {
            printf ("Frame #%u:\n", zmv_open_vars.frames_replayed);
            printf ("Pad A input: %s\n",   decode_input(zmv_vars.last_joy_state.A));
            printf ("Pad B input: %s\n",   decode_input(zmv_vars.last_joy_state.B));
            printf ("Pad C input: %s\n",   decode_input(zmv_vars.last_joy_state.C));
            printf ("Pad D input: %s\n",   decode_input(zmv_vars.last_joy_state.D));
            printf ("Pad E input: %s\n\n", decode_input(zmv_vars.last_joy_state.E));
          }
        }
        else
        {
          if (flag & 0xF8)
          {
            printf ("Frame #%u:\n", zmv_open_vars.frames_replayed);

            if (flag & 0x80)	{ printf ("Pad A input: %s\n", decode_input(zmv_vars.last_joy_state.A)); }
            if (flag & 0x40)	{ printf ("Pad B input: %s\n", decode_input(zmv_vars.last_joy_state.B)); }
            if (flag & 0x20)	{ printf ("Pad C input: %s\n", decode_input(zmv_vars.last_joy_state.C)); }
            if (flag & 0x10)	{ printf ("Pad D input: %s\n", decode_input(zmv_vars.last_joy_state.D)); }
            if (flag & 0x08)	{ printf ("Pad E input: %s\n", decode_input(zmv_vars.last_joy_state.E)); }

            puts("");
          }
        }
      }
    }
  }
}

static void set_author(const unsigned char *name)
{
  unsigned short nsize = strlen(name);

  zmv_vars.header.author_len = nsize;
  fseek(zmv_vars.fp, AUTHOR_LEN_LOC, SEEK_SET);
  fwrite(&nsize, 1, 2, zmv_vars.fp);
  fseek(zmv_vars.fp, 0, SEEK_END);
  fwrite(name, 1, nsize, zmv_vars.fp);

  printf("Result - ");
  display_author();
  puts("");
}

static void decode_input2(unsigned char *array, unsigned int input)
{
  memset(array, 0, 29);
  input >>= 20;
/*
  d-pad possibilites:
  NULL, UP, DOWN, LEFT, RIGHT, UP-LEFT, UP-RIGHT, DOWN-LEFT, DOWN-RIGHT
  LEFT-RIGHT, DOWN-L-R, UP-L-R, UP-DOWN, U-D-RIGHT, U-D-LEFT, U-D-L-R
*/
  switch (input & 0x0F0)
  {
    case 0x010:
      strcpy(array, "Right");
      break;
    case 0x020:
      strcpy(array, "Left");
      break;
    case 0x030:
      strcpy(array, "L+R");
      break;
    case 0x040:
      strcpy(array, "Down");
      break;
    case 0x050:
      strcpy(array, "Down-Right");
      break;
    case 0x060:
      strcpy(array, "Down-Left");
      break;
    case 0x070:
      strcpy(array, "Down-L-R");
      break;
    case 0x080:
      strcpy(array, "Up");
      break;
    case 0x090:
      strcpy(array, "Up-Right");
      break;
    case 0x0A0:
      strcpy(array, "Up-Left");
      break;
    case 0x0B0:
      strcpy(array, "Up-L-R");
      break;
    case 0x0C0:
      strcpy(array, "Up-Down");
      break;
    case 0x0D0:
      strcpy(array, "U-D-Right");
      break;
    case 0x0E0:
      strcpy(array, "U-D-Left");
      break;
    case 0x0F0:
      strcpy(array, "U-D-L-R");
      break;
    default:
      strcpy(array, "Null");
  }

  if (input & 0x100)	{ strcat(array, " St"); }
  if (input & 0x200)	{ strcat(array, " Sl"); }
  if (input & 0x008)	{ strcat(array, " A"); }
  if (input & 0x800)	{ strcat(array, " B"); }
  if (input & 0x004)	{ strcat(array, " X"); }
  if (input & 0x400)	{ strcat(array, " Y"); }
  if (input & 0x002)	{ strcat(array, " L"); }
  if (input & 0x001)	{ strcat(array, " R"); }
}

static void buffer_write(const unsigned int startframe, const unsigned int duration, const unsigned char *input_string, FILE *fp)
{
  if ((duration) && (startframe))
  {
    fprintf(fp, "%u:%u:%s\n", startframe, duration-1, input_string);
  }
}

static void autosubtitlor(unsigned char *moviefname, const unsigned char pad)
{
  FILE *subfp;
  unsigned int store_frame = 0;
  unsigned char subfname[512];
  unsigned char buffer[29] = "Dpad-Input St Sl A B X Y L R";
  bool task_done = false;

  memcpy(subfname, moviefname, strlen(moviefname));
  memset(subfname+strlen(moviefname), 0, 1);
  if (subfname[strlen(moviefname)-3] == 'Z')
      { memcpy(subfname+strlen(moviefname)-3, "SU", 2); }
  if (subfname[strlen(moviefname)-1] == 'V')
	{ memset(subfname+strlen(moviefname)-1, 'B', 1); }
  if (subfname[strlen(moviefname)-3] == 'z')
      { memcpy(subfname+strlen(moviefname)-3, "su", 2); }
  if (subfname[strlen(moviefname)-1] == 'v')
	{ memset(subfname+strlen(moviefname)-1, 'b', 1); }

  if ((subfp = fopen(subfname, "w")))
  {
    zmv_open_vars.frames_replayed = 0;
    zmv_vars.last_joy_state.A = 0;
    zmv_vars.last_joy_state.B = 0;
    zmv_vars.last_joy_state.C = 0;
    zmv_vars.last_joy_state.D = 0;
    zmv_vars.last_joy_state.E = 0;

    while (!task_done)
    {
      if (!zmv_replay())
      {
        buffer_write(store_frame, zmv_open_vars.frames_replayed - store_frame
                      , buffer, subfp);
        task_done = true;
      }
      else
      {
        if (flag & (0x80 >> pad))
        {
          buffer_write(store_frame, zmv_open_vars.frames_replayed - store_frame
                      , buffer, subfp);

          if (pad == 0)	{ decode_input2(buffer, zmv_vars.last_joy_state.A); }
          if (pad == 1)	{ decode_input2(buffer, zmv_vars.last_joy_state.B); }
          if (pad == 2)	{ decode_input2(buffer, zmv_vars.last_joy_state.C); }
          if (pad == 3)	{ decode_input2(buffer, zmv_vars.last_joy_state.D); }
          if (pad == 4)	{ decode_input2(buffer, zmv_vars.last_joy_state.E); }

          store_frame = zmv_open_vars.frames_replayed;
        }
      }
    }

    fclose(subfp);
    printf("Pad %c input subtitle file successfully autogenerated:\n %s\n\n"
	, pad+'A', subfname);
  }
  else  { puts("Error creating subtitle file.\n"); }
}

static bool isnumber (const char *array)
{
  unsigned int i = 0;

  do { if ((array[i] < '0') || (array[i] > '9')) { return (false); }
  } while (array[++i]);

  return (true);
}

static unsigned int string2value (const unsigned char *string)
{
  unsigned int value = 0, i = 0;

  while (string[i]) { value = value*10 + (string[i++] - '0'); }

  return (value);
}

static unsigned char alpha2value (const unsigned char letter)
{
  unsigned char value = ~0;

  if ((letter >= 'a') && (letter <= 'z'))	{ value = letter - 'a'; }
  if ((letter >= 'A') && (letter <= 'Z'))	{ value = letter - 'A'; }

  return (value);
}

int main(int argc, char **argv)
{
  printf("Enhanced ZMV Parser %s      Copyright (c) 2005      ZSNES Team\n\n", EZP_VERSION);

  if (argc < 2)
  {
    puts("Usage:\tezp <params>[<value>] <zmv filename>\n\n");
    puts("<Params>");
    puts(" -co:        - chapter offsets sorted by ascending frame #");
//    puts(" -cd:        - interactive deletion of chapters - not implemented yet");
    puts(" -fc:        - pad input changelog");
    puts(" -ff:        - pad input full log");
    puts(" -fi value:  - pad input for frame #value");
    puts(" -an name:   - set name as movie author - only for anonymous movies");
    puts(" -as letter: - auto pad #letter input subtitle generator\n");
    puts("Note: the -fc/ff params output should be piped into less or cat\n");
    puts("  Default: no params - only displays basic header/footer info\n");

    return (1);
  }
  else
  {
    if (zmv_open(argv[argc-1]))
    {
      unsigned int i = 1;
      bool invalid;

      puts("Valid enhanced ZMV detected.");
      printf("Filename: %s\n\n", argv[argc-1]);

      basic_info();

      for (; i<argc-1 ; i++)
      {
        fseek(zmv_vars.fp, zmv_open_vars.input_start_pos, SEEK_SET);
        invalid = true;

        if (!strcmp(argv[i], "-co"))
        {
          chapter_info();
          invalid = false;
        }
/*        if (!strcmp(argv[i], "-cd"))
        {
          dechapterizator();
          zmv_replay_finished();
          zmv_open(argv[argc-1]);
          chapter_info();
          invalid = false;
        }*/
        if (!strcmp(argv[i], "-fc"))
        {
          input_info(0, false);
          invalid = false;
        }
        if (!strcmp(argv[i], "-ff"))
        {
          input_info(0, true);
          invalid = false;
        }
        if ((!strcmp(argv[i], "-fi")) && (i < argc-1))
        {
          i++;
          if ((isnumber(argv[i])) && (string2value(argv[i])))
          { input_info(string2value(argv[i]), true); }
          else	{ puts("Invalid argument for parameter -fi\n"); }
          invalid = false;
        }
        if ((!strcmp(argv[i], "-an")) && (i < argc-1))
        {
          i++;
          if (!zmv_vars.header.author_len)
          { set_author(argv[i]); }
          else	{ puts("Movie already signed\n"); }
          invalid = false;
        }
        if ((!strcmp(argv[i], "-as")) && (i < argc-1))
        {
          i++;
          if (alpha2value(*argv[i]) < 5)
          { autosubtitlor(argv[argc-1], alpha2value(*argv[i])); }
          else	{ puts("Invalid argument for parameter -as\n"); }
          invalid = false;
        }
        if (invalid)	{ printf("Invalid parameter '%s'\n\n", argv[i]); }
      }

      zmv_replay_finished();

      return (0);
    }
    else
    {
      printf("%s is not a valid enhanced ZMV file.\n\n", argv[argc-1]);
      return (-1);
    }
  }
}
