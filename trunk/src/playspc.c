// testbench for SPC code without all that distracting emulator to get in the way

// (if we want to offer ZSPC as a generally useful SPC core, this
// file should probably get a lot smaller & lose the ASM)


#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ao/ao.h>
#include "cpu/dspwrap.h"
#include "cpu/dsp.h"
#include "asm_call.h"

int romispal = 0, spc700read = 0, cycpbl = 0, cycpblt = 0, curexecstate = 0;
void *tableadc[0];
int disablespcclr = 0, SPCSkipXtraROM;

void (*opcjmptab[256])(void);

extern unsigned char SPCRAM[0x10000];
extern unsigned char* spcPCRam;
extern unsigned char spcextraram[64];
extern int spcCycle,lastCycle;
extern unsigned char timeron, timincr0, timincr1, timincr2;
extern unsigned char spcA, spcX, spcY, spcP, spcNZ, spcS;
extern void InitSPC();
extern void catchup();

struct header_t
{
  char          tag[33];
  unsigned char marker[3];
  unsigned char marker2;
  unsigned char pc[2];
  unsigned char a, x, y, statflags, stack;
  unsigned char reserved[2];
  char          song[32];
  char          game[32];
  char          dumper[16];
  char          comment[32];
  unsigned char date[4];
  unsigned char reserved2[7];
  unsigned char len_secs[4];
  unsigned char fade_msec[3];
  char          author[32];
  unsigned char mute_mask;
  unsigned char emulator;
  unsigned char reserved3[46];
};

static unsigned char DSPRegs[0x80];

static void read_spcfile(const char *fname, struct header_t *header)
{
  FILE *fp = fopen(fname, "rb");
  fread(header, sizeof(struct header_t), 1, fp);
  fread(SPCRAM, sizeof(SPCRAM), 1, fp);
  fread(DSPRegs, sizeof(DSPRegs), 1, fp);
  fseek(fp, (256-sizeof(spcextraram))-sizeof(DSPRegs), SEEK_CUR);
  fread(spcextraram, sizeof(spcextraram), 1, fp);
  fclose(fp);
}

static void print_spcinfo(struct header_t *header)
{
  char *emulator;
  switch (header->emulator)
  {
    case 1:
      emulator = "ZSNES";
      break;
    case 2:
      emulator = "Snes9x";
      break;
    default:
      emulator = "Unknown";
      break;
  }
  printf("Emulator: %s\n  Dumper: %s\n    Game: %s\n   Title: %s\n  Artist: %s\n\n", emulator, header->dumper, header->game, header->song, header->author);
}

static void init_spc(struct header_t *header)
{
  unsigned int i;

  spcPCRam = SPCRAM+(header->pc[0]+(header->pc[1]<<8));
  spcA = header->a;
  spcX = header->x;
  spcY = header->y;
  spcP = header->statflags;
  spcNZ = (spcP & 0x82)^2;
  spcS = 0x100+header->stack;

  timeron = SPCRAM[0xf1] & 7;
  timincr0 = SPCRAM[0xfa];
  timincr1 = SPCRAM[0xfb];
  timincr2 = SPCRAM[0xfc];

  for (i = 0; i < sizeof(DSPRegs); i++)
  {
    dsp_write(i, DSPRegs[i]);
  }
}

ao_device *ao_init()
{
  ao_device *dev;
  struct ao_sample_format format = { 16, 32000, 2, AO_FMT_LITTLE };

  ao_initialize();
  atexit(ao_shutdown);
  dev = ao_open_live(ao_default_driver_id(), &format, NULL);

  if (dev)
  {
    ao_info *di = ao_driver_info(dev->driver_id);
    printf("\nAudio Opened.\nDriver: %s\nChannels: %u\nRate: %u\n\n", di->name, format.channels, format.rate);
  }

  return(dev);
}

void run_spc(ao_device *dev)
{
  if (dev)
  {
    for (;;)
    {
      asm_call(catchup);
      asm_call(updatetimer);
      dsp_run_wrap();
      if (dsp_sample_count > 128)
      {
        extern short dsp_samples_buffer[];
        if (!ao_play(dev, (char*)dsp_samples_buffer, dsp_sample_count*2))
        {
          exit(-1);
        }
        dsp_sample_count = 0;
      }
      // printf("PC = 0x%04x\n", spcPCRam-SPCRAM);
    }
  }
}

int main(int argc, char *argv[])
{
  struct header_t header;

  assert(sizeof(struct header_t) == 0x100);

  InitSPC();
  read_spcfile(argv[1], &header);
  print_spcinfo(&header);
  init_spc(&header);
  run_spc(ao_init());

  return(0);
}
