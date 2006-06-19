
#include <string.h>
#include <ctype.h>
#include <curses.h>
#include "zpath.h"

#ifdef __MSDOS__
#include <dpmi.h>
#endif // __MSDOS__

#include "asm_call.h"

// All of these should be in headers, people!

extern unsigned char curblank;
extern unsigned char curcyc;
extern unsigned char curypos;
extern unsigned char CurrentCPU;

extern unsigned char soundon;
extern unsigned int  cycpbl;

extern unsigned short xpc, xa, xx, xy, xs, xd;
extern unsigned char  xpb, xdb, xp, xe;

extern void *snesmmap[256];
extern void *snesmap2[256];
extern char *dmadata;

extern void (*memtabler8[256])();
extern unsigned char memtabler8_wrapper(unsigned char, unsigned short);

extern void regaccessbankr8();
extern void start65816();

char *ocname;
unsigned char addrmode[];

/*
unsigned short debugh  = 0; // debug head
unsigned short debugt  = 0; // debug tail
unsigned short debugv  = 0; // debug view
*/
unsigned char  debugds = 0; // debug disable (bit 0 = 65816, bit 1 = SPC)
unsigned int   numinst = 0; // # of instructions

unsigned char wx = 0, wy = 0, wx2 = 0, wy2 = 0;
unsigned char execut = 0; 
unsigned char debuggeron = 1;
unsigned char debstop = 0, debstop2 = 0, debstop3 = 0, debstop4 = 0;

char debugsa1 = 0;
char skipdebugsa1 = 1;

#define CP(n) (A_BOLD | COLOR_PAIR(n))

enum color_pair {
    cp_white = 1, cp_magenta, cp_red, cp_cyan, cp_green, cp_yellow,
    cp_white_on_blue,
}; 

WINDOW *debugwin;

// can't get this to work right...
//#define CHECK (COLOR_PAIR(cp_white) | A_DIM | ACS_CKBOARD)
#define CHECK (CP(cp_white) | ' ')

void debugloop();

void startdisplay();
void nextopcode();
void cleardisplay();
void nextspcopcode();
void SaveOAMRamLog();
extern void execnextop();

unsigned char *findop();
unsigned char *findoppage();

void startdebugger() {
    static int firsttime = 1;

    curblank = 0x40;
    debuggeron = 1;
    
#ifdef __MSDOS__
    __dpmi_regs regs;
    regs.x.ax = 0x0003;
    __dpmi_int(0x10, &regs);
#endif // __MSDOS__
            
    if (firsttime) {
    initscr(); cbreak(); noecho();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);

    /* set up colors */
    start_color();
    init_pair(cp_white,         COLOR_WHITE,   COLOR_BLACK);
    init_pair(cp_magenta,       COLOR_MAGENTA, COLOR_BLACK);
    init_pair(cp_red,           COLOR_RED,     COLOR_BLACK);
    init_pair(cp_cyan,          COLOR_CYAN,    COLOR_BLACK);
    init_pair(cp_green,         COLOR_GREEN,   COLOR_BLACK);
    init_pair(cp_yellow,        COLOR_YELLOW,  COLOR_BLACK);
    init_pair(cp_white_on_blue, COLOR_WHITE,   COLOR_BLUE);
    }
    
    execut = 0;

    if (firsttime) {
    startdisplay();

    debugwin = newwin(20, 77, 2, 1);
    
    wbkgd(debugwin, CP(cp_white_on_blue) | ' ');
    wattrset(debugwin, CP(cp_white_on_blue));

    scrollok(debugwin, TRUE);
    idlok(debugwin, TRUE);
    
    firsttime = 0;
    }

    debugloop();
    cleardisplay();

    // "pushad / call LastLog / ... / popad" elided
    SaveOAMRamLog();

    
    if (execut == 1) {
	start65816(); return;
    }
  
}

//*******************************************************
// Debug Loop
//*******************************************************

void debugloop() {
  a:
    if (!(debugds & 2))
	nextopcode();
    if (!(debugds & 1))
	nextspcopcode();
  
  b:;
   int key = getch();
   if (key >= 0 && key < 256)
       key = toupper(key);
   switch (key) {
   case KEY_F(1):
       execut = 1;
       return;
   case 27:
       return;
   case '\n':
       goto e;
   default:
       wprintw(debugwin, "Unknown key code: %d\n", key);
       goto b;
   }
  e:
   skipdebugsa1 = 0;
   asm_call(execnextop);
   skipdebugsa1 = 1;
   if (soundon && (debugds & 2) && (cycpbl <= 55))
       goto e;
   goto a;
	
}

//*******************************************************
// BreakatSignB               Breaks whenever keyonsn = 1
//*******************************************************
char keyonsn = 0;
char prbreak = 0;

/* extern FOO SPCSave */

void breakatsignb() {
    // ...
}


//*******************************************************
// BreakatSignC               Breaks whenever sndwrit = 1
//*******************************************************

unsigned char sndwrit;

/* void breakatsignc() {} */


void printinfo(char *s) {
    while (s[0]) {
	if (s[0] == '@') {
	    int colors[] = {
		0, 0, cp_green, cp_cyan,
		cp_red, cp_magenta, cp_yellow, cp_white
	    };
	    attrset(COLOR_PAIR(colors[s[1]-'0']));
	    s += 2;
	} else {
	    addch(s[0]);
	    s += 1;
	}
    }
}

/* Won't port too well - stuck it in debugasm.asm for now */
/* void execnextop() { */
/*     char *page = findoppage(); */
/*     initaddrl = page; */
/*     char *address = page+xpc; */
/* } */

//*******************************************************
// Start Display
//*******************************************************
void startdisplay() {
    int i;

    // Clear the screen
    bkgd(CP(cp_white) | ' ');
    clear();

    // Draw to screen

    // ASM for some reason puts the same thing in the upper left corner again?

    move(1, 0); attrset(CP(cp_white_on_blue));
    addch(CurrentCPU+'0');
    for (i = 15; i; i--) 
	addch(ACS_HLINE);
    printw(" CC:    Y:    ");
    for (i = 19; i; i--) 
	addch(ACS_HLINE);
    addch(' ');
    for (i = 11; i; i--) 
	addch(' ');
    addch(' ');
    for (i = 16; i; i--) 
	addch(ACS_HLINE);
    addch(ACS_URCORNER);

    for (i = 2; i < 22; i++) {
	mvaddch(i, 0, ACS_VLINE);
	hline(' ', 77);
	mvaddch(i, 78, ACS_VLINE);
	mvaddch(i, 79, CHECK);
    }
    
    mvaddch(22, 0, ACS_LLCORNER);
    for(i = 77; i; i--)
	addch(ACS_HLINE);
    mvaddch(22, 78, ACS_LRCORNER);
    mvaddch(22, 79, CHECK);
    
    move(23, 1);
    for(i = 79; i; i--) 
	addch(CHECK);
    
    // Print debugger information
    
    move(0, 2); attrset(CP(cp_white));
    printinfo("- @5Z@4S@3N@2E@6S@7 debugger -");

    move(1, 4); attrset(CP(cp_white_on_blue));
    printinfo(" 65816 ");
    
    // HACK ALERT! this should really be on the bottom line, but
    // xterms default to being one line shorter than 80x25, so this
    // won't be on the bottom line on DOS!
    // Also, we are printing on top of the (currently invisible) drop shadow!"
    move(23, 0);
    printinfo("@4(@6T@4)@7race for  @4(@6B@4)@7reakpoint  "
	      "@4(@6Enter@4)@7 Next  "
	      "@4(@6M@4)@7odify  @4(@6F9@4)@7 Signal  @4(@6F1@4)@7 Run");

    // ...
    move(0, 0);
    refresh();
}


//*******************************************************
// Next Opcode              Writes the next opcode & regs
//*******************************************************
// 008000 STZ $123456,x A:0000 X:0000 Y:0000 S:01FF DB:00 D:0000 P:33 E+

/*
void addtail() {
    debugt++;
    if (debugt == 100)
	debugt = 0;
    if (debugt == debugh)
	debugh++;
    if (debugh == 100)
	debugh = 0;
}
*/


void out65816_addrmode (unsigned char *instr) {
    int i;
    char *padding = "";

    inline unsigned char getxb() {
	if (ocname[4*instr[0]] != 'J')
	    return xdb;
	else
	    return xpb;
    }

    // each mode must output 19 characters
    switch (addrmode[instr[0]]) {

    case 0:
    case 6:
    case 21:
	// nothing to show

	wprintw(debugwin, "%19s", padding);
	break;
	
    case 1:     // #$12,#$1234 (M-flag)
	wprintw(debugwin, "#$");
	if (xp != 0x20) {
	    wprintw(debugwin, "%02x", instr[1]);
	    wprintw(debugwin, "%15s", padding);
	} else {
	    wprintw(debugwin, "%04x", *(unsigned short *)(instr+1));
	    wprintw(debugwin, "%13s", padding);
    	}
	break;
	
    case 2:     // $1234 : db+$1234
	wprintw(debugwin, "$%04x", *(unsigned short *)(instr+1));
	wprintw(debugwin, "%5s[%02x%04x] ", padding, getxb(),
		                            *(unsigned short *)(instr+1));
	break;
	
    case 3:     // $123456
	wprintw(debugwin, "$%02x%04x", instr[3], *(unsigned short*)(instr+1));
	wprintw(debugwin, "%12s", padding);
	break;

    case 4:     // $12 : $12+d
	wprintw(debugwin, "$%02x%7s[%02x%04x] ", instr[1], padding, 0,
		                                 instr[1]+xd);
	break;

    case 5:     // A
	wprintw(debugwin, "A%18s", padding);
	break;

    case 14:    // $123456,x : $123456+x
    {
	unsigned int t = instr[1] | (instr[2] << 8) | (instr[3] << 16);
	wprintw(debugwin, "$%06x,X ", t);
	if (xp & 0x10) {
	    t = (t & ~0xff)   | ((t + xx) & 0xff);
	} else {
	    t = (t & ~0xffff) | ((t + xx) & 0xffff);
	}
	wprintw(debugwin, "[%06x] ", t);
	
	
	break;
    }

    case 15:    // +-$12 / $1234
    {   
	char c = instr[1];
	unsigned short t = c + xpc + 2;
	wprintw(debugwin, "$%04x%4s [%02x%04x] ", t, padding, xpb, t);
	break;
    }

    case 20:    // ($1234,x)
    {
	unsigned short cx = *(unsigned short*)(instr+1);
	wprintw(debugwin, "($%04x,X) [%02x", cx, xpb);
	if (xp & 0x10) 
	    cx = (cx & 0xFF00) | ((cx + xx) & 0xFF);
	else
	    cx += xx;
	// .out20n
	// next part baffles me!
	unsigned short x;
	x = memtabler8_wrapper(xpb, cx);
	x += memtabler8_wrapper(xpb, cx+1) << 8;
	wprintw(debugwin, "%04x] ", x);

	break;
    }	
   

    case 25:    // #$12 (Flag Operations)
	wprintw(debugwin, "#$%02x%15s", instr[1], padding);
	break;

    case 26:    // #$12,#$1234 (X-flag)
	if (xp & 0x10) {
	    wprintw(debugwin, "#$%02x%15s", instr[1], padding);
	} else {
	    wprintw(debugwin, "#$%04x%13s",
		    *(unsigned short*)(instr+1), padding);
	}
	break;

    default:
	wprintw(debugwin, "%15s %02d ", "bad addr mode", addrmode[instr[0]]);
    }
}

unsigned char *findoppage() {
    if (xpc & 0x8000) {
	return snesmmap[xpb];
    } else {
	// lower address
	if ((xpc < 0x4300) && (memtabler8[xpb] != regaccessbankr8)) {
	    // lower memory
	    return snesmap2[xpb];
	} else {
	    // dma
	    return (unsigned char*)&dmadata[xpb-0x4300];
	}
    }    
}

unsigned char *findop() {
    unsigned char *address = findoppage()+xpc;
    return address;
}

// print out a 65816 instruction
void out65816() {
    wprintw(debugwin, "%02x%04x ", xpb, xpc);

    // is this safe?
    unsigned char *address = findop();
    unsigned char opcode = *address;
    
    char opname[5] = "FOO ";
    memcpy(opname, &ocname[opcode*4], 4);
    wprintw(debugwin, "%s", opname);

    out65816_addrmode(address);

    wprintw(debugwin, "A:%04x X:%04x Y:%04x S:%04x DB:%02x D:%04x P:%02x %c",
	    xa, xx, xy, xs, xdb, xd, xp, (xe == 1) ? 'E' : 'e');
}

void outsa1() {
    // stub!
}


void nextopcode() {
    attrset(CP(cp_white_on_blue));

    move(1,50); printw("%11d", numinst);
    move(1,20); printw("%3d",  curcyc);
    move(1,26); printw("%3d",  curypos);

    // I don't understand the buffering scheme here... I'm just going
    // to hope it isn't really all that important.

    //if (debugsa1 != 1)
	out65816();
    //else 
    //	outputbuffersa1();

    // redrawing the display is always a good idea
    wrefresh(debugwin);
    refresh();
}


void cleardisplay() {
    move(0, 0); /* clear(); refresh(); */ endwin();
}

//*******************************************************
// Next SPC Opcode          Writes the next opcode & regs
//*******************************************************
// 008000 STZ $123456,x A:0000 X:0000 Y:0000 S:01FF DB:00 D:0000 P:33 E+
void nextspcopcode() {
    // ...
}

//*******************************************************
// Debugger OpCode Information
//*******************************************************

char *ocname =
"BRK ORA COP ORA TSB ORA ASL ORA PHP ORA ASL PHD TSB ORA ASL ORA "
"BPL ORA ORA ORA TRB ORA ASL ORA CLC ORA INC TCS TRB ORA ASL ORA "
"JSR AND JSL AND BIT AND ROL AND PLP AND ROL PLD BIT AND ROL AND "
"BMI AND AND AND BIT AND ROL AND SEC AND DEC TSC BIT AND ROL AND "
"RTI EOR WDM EOR MVP EOR LSR EOR PHA EOR LSR PHK JMP EOR LSR EOR "
"BVC EOR EOR EOR MVN EOR LSR EOR CLI EOR PHY TCD JMP EOR LSR EOR "
"RTS ADC PER ADC STZ ADC ROR ADC PLA ADC ROR RTL JMP ADC ROR ADC "
"BVS ADC ADC ADC STZ ADC ROR ADC SEI ADC PLY TDC JMP ADC ROR ADC "
"BRA STA BRL STA STY STA STX STA DEY BIT TXA PHB STY STA STX STA "
"BCC STA STA STA STY STA STX STA TYA STA TXS TXY STZ STA STZ STA "
"LDY LDA LDX LDA LDY LDA LDX LDA TAY LDA TAX PLB LDY LDA LDX LDA "
"BCS LDA LDA LDA LDY LDA LDX LDA CLV LDA TSX TYX LDY LDA LDX LDA "
"CPY CMP REP CMP CPY CMP DEC CMP INY CMP DEX WAI CPY CMP DEC CMP "
"BNE CMP CMP CMP PEI CMP DEC CMP CLD CMP PHX STP JML CMP DEC CMP "
"CPX SBC SEP SBC CPX SBC INC SBC INX SBC NOP XBA CPX SBC INC SBC "
"BEQ SBC SBC SBC PEA SBC INC SBC SED SBC PLX XCE JSR SBC INC SBC ";


// Immediate Addressing Modes :
//   09 - ORA-M, 29 - AND-M, 49 - EOR-M, 69 - ADC-M, 89 - BIT-M,
//   A0 - LDY-X, A2 - LDX-X, A9 - LDA-M, C0 - CPY-X, C2 - REP-B,
//   C9 - CMP-M, E0 - CPX-X, E2 - SEP-B, E9 - SBC-M
//   Extra Addressing Mode Values : B(1-byte only) = 25, X(by X flag) = 26

unsigned char addrmode[256] = {
    25, 9,25,22, 4, 4, 4,19,21, 1, 5,21, 2, 2, 2, 3,
    15, 7,18,23, 4,10,10, 8, 6,13, 5, 6, 2,12,12,14,
     2, 9, 3,22, 4, 4, 4,19,21, 1, 5,21, 2, 2, 2, 3,
    15, 7,18,23,10,10,10, 8, 6,13, 5, 6,12,12,12,14,
    21, 9, 0,22,24, 4, 4,19,21, 1, 5,21, 2, 2, 2, 3,
    15, 7,18,23,24,10,10, 8, 6,13,21, 6, 3,12,12,14,
    21, 9, 2,22, 4, 4, 4,19,21, 1, 5,21,17, 2, 2, 3,
    15, 7,18,23,10,10,10, 8, 6,13,21, 6,20,12,12,14,
    15, 9,16,22, 4, 4, 4,19, 6, 1, 6,21, 2, 2, 2, 3,
    15, 7,18,23,10,10,11, 8, 6,13, 6, 6, 2,12,12,14,
    26, 9,26,22, 4, 4, 4,19, 6, 1, 6,21, 2, 2, 2, 3,
    15, 7,18,23,10,10,11, 8, 6,13, 6, 6,12,12,13,14,
    26, 9,25,22, 4, 4, 4,19, 6, 1, 6, 6, 2, 2, 2, 3,
    15, 7,18,23,18,10,10, 8, 6,13,21, 6,27,12,12,14,
    26, 9,25,22, 4, 4, 4,19, 6, 1, 6, 6, 2, 2, 2, 3,
    15, 7,18,23, 2,10,10, 8, 6,13,21, 6,20,12,12,14
};

extern unsigned char oamram[1024], SPCRAM[65472], DSPMem[256];

void SaveOAMRamLog() {
  FILE *fp = 0;

  if ((fp = fopen_dir(ZCfgPath,"vram.dat","wb"))) {
    fwrite(oamram,1,544,fp);
    fclose(fp);
  }
}

void debugdump() {
  FILE *fp = 0;

  if ((fp = fopen_dir(ZCfgPath,"SPCRAM.dmp","wb"))) {
    fwrite(SPCRAM,1,65536,fp);
    fclose(fp);
  }

  if ((fp = fopen_dir(ZCfgPath,"DSP.dmp","wb"))) {
    fwrite(DSPMem,1,256,fp);
    fclose(fp);
  }
}
