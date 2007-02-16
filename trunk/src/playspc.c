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

int romispal = 0, spc700read = 0, cycpbl = 0, cycpblt = 0, curexecstate = 0;
void *tableadc[0];
int disablespcclr = 0, SPCSkipXtraROM;

void (*opcjmptab[256])(void);

extern unsigned char SPCRAM[0x10000];
extern unsigned char* spcPCRam;
extern unsigned char spcextraram[64];
extern unsigned char timeron, timincr0, timincr1, timincr2;
extern unsigned char spcA, spcX, spcY, spcP, spcNZ, spcS;
extern int DSP_midframe;
extern void InitSPC();

ao_device *dev;

struct header_t {
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

int main(int argc, char *argv[]) {
    unsigned int i;

    struct ao_sample_format format = { 16, 32000, 2, AO_FMT_LITTLE };

    FILE *fin;
    struct header_t header;
    unsigned char DSPRegs[0x80];
    char *emulator;

    assert(sizeof(header) == 0x100);

    InitSPC();

    fin = fopen(argv[1], "r");
    fread(&header, sizeof(header), 1, fin);
    fread(SPCRAM, sizeof(SPCRAM), 1, fin);
    fread(DSPRegs, sizeof(DSPRegs), 1, fin);
    fseek(fin, (256-sizeof(spcextraram))-sizeof(DSPRegs), SEEK_CUR);
    fread(spcextraram, sizeof(spcextraram), 1, fin);
    fclose(fin);

    spcPCRam = SPCRAM+(header.pc[0]+(header.pc[1]<<8));
    spcA = header.a;
    spcX = header.x;
    spcY = header.y;
    spcP = header.statflags;
    spcNZ = (spcP & 0x82)^2;
    spcS = 0x100+header.stack;

    switch (header.emulator) {
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

    timeron = SPCRAM[0xf1] & 7;
    timincr0 = SPCRAM[0xfa];
    timincr1 = SPCRAM[0xfb];
    timincr2 = SPCRAM[0xfc];

    for (i = 0; i < sizeof(DSPRegs); i++)
    {
      dsp_write(i, DSPRegs[i]);
    }


    ao_initialize();
    dev = ao_open_live(ao_driver_id("oss"), &format, NULL);

    if (dev) {
        ao_info *di = ao_driver_info(dev->driver_id);
        printf("\nAudio Opened.\nDriver: %s\nChannels: %u\nRate: %u\n\n", di->name, format.channels, format.rate);
        printf("Emulator: %s\n  Dumper: %s\n    Game: %s\n   Title: %s\n  Artist: %s\n\n", emulator, header.dumper, header.game, header.song, header.author);
    } else {
        exit(1);
    }

    while (1) {
        __asm__(
       ".intel_syntax       \n\
        pushad              \n\
        mov %ebp,[spcPCRam] \n\
        call updatetimer    \n\
        call catchup        \n\
        mov [spcPCRam],%ebp \n\
        popad               \n\
        .att_syntax"
            );
        //DSP_midframe = 0;
        dsp_run_wrap();
        DSP_midframe = 1;
        // printf("PC = 0x%04x\n", spcPCRam-SPCRAM);
    }
}

void SoundWrite_ao() {}

void write_audio(short *sample_buffer, size_t sample_count) {
    if (!ao_play(dev, (char*)sample_buffer, sample_count*2))
        exit(1);
}

