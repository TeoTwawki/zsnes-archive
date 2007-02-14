// testbench for SPC code without all that distracting emulator to get in the way

// (if we want to offer ZSPC as a generally useful SPC core, this
// file should probably get a lot smaller & lose the ASM)


#include <stdlib.h>
#include <stdio.h>
#include <ao/ao.h>
#include "cpu/dspwrap.h"
#include "cpu/dsp.h"

int romispal = 0, spc700read = 0, cycpbl = 0, cycpblt = 0, curexecstate = 0;
void *tableadc[0];
int disablespcclr = 0, SPCSkipXtraROM;

void (*opcjmptab[256])(void);

extern unsigned char SPCRAM[];
extern unsigned char* spcPCRam;
extern unsigned char spcextraram[];
extern unsigned char timincr0, timincr1, timincr2;
extern unsigned int spcA, spcX, spcY, spcP, spcNZ, spcS;
extern int DSP_midframe;
extern void InitSPC();

ao_device *dev;

int main(int argc, char *argv[]) {
    int i;

    struct ao_sample_format format = { 16, 32000, 2, AO_FMT_LITTLE };

    FILE *fin;
    unsigned char header[0x100];
    unsigned char DSPRegs[0x100];
    unsigned char junk[0x40];

    InitSPC();

    fin = fopen(argv[1], "r");
    fread(header, 0x100, 1, fin);
    fread(SPCRAM, 0x10000, 1, fin);
    fread(DSPRegs, 0x80, 1, fin);
    fread(junk, 0x40, 1, fin);
    fread(spcextraram, 0x40, 1, fin);

    spcPCRam = SPCRAM+(header[0x25]+(header[0x26]<<8));
    spcA = header[0x27];
    spcX = header[0x28];
    spcY = header[0x29];
    spcP = header[0x2A] & 0xFD;
    spcNZ = -(header[0x2A]&2);
    spcS = 0x100+header[0x2B];

    timincr0 = SPCRAM[0xfa];
    timincr1 = SPCRAM[0xfb];
    timincr2 = SPCRAM[0xfc];

    for (i = 0; i < 128; i++)
	dsp_write(i, DSPRegs[i]);

    
    ao_initialize();
    dev = ao_open_live(ao_driver_id("oss"), &format, NULL);

    if (dev) {
	ao_info *di = ao_driver_info(dev->driver_id);
	printf("\nAudio Opened.\nDriver: %s\nChannels: %u\nRate: %u\n\n", di->name, format.channels, format.rate);
    } else {
	exit(1);
    }

    while (1) {
	__asm__(
       ".intel_syntax       \n\
	pushad              \n\
        mov %ebp,[spcPCRam] \n\
        call updatetimer    \n\
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

