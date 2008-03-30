;Copyright (C) 1997-2008 ZSNES Team ( zsKnight, _Demo_, pagefault, Nach )
;
;http://www.zsnes.com
;http://sourceforge.net/projects/zsnes
;https://zsnes.bountysource.com
;
;This program is free software; you can redistribute it and/or
;modify it under the terms of the GNU General Public License
;version 2 as published by the Free Software Foundation.
;
;This program is distributed in the hope that it will be useful,
;but WITHOUT ANY WARRANTY; without even the implied warranty of
;MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;GNU General Public License for more details.
;
;You should have received a copy of the GNU General Public License
;along with this program; if not, write to the Free Software
;Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.



%include "macros.mac"

EXTSYM disableeffects,winl1,winl2,winbgdata,winr1,winr2,winspdata,winlogica
EXTSYM winenabm,winobjen,winlogicb,scrndis,scrnon,bgmode,bgtilesz,winenabs
EXTSYM bg1objptr,bg1ptr,bg1ptrb,bg1ptrc,bg1ptrd,bg1scrolx,bg1scroly,cachebg1
EXTSYM curbgofs1,curcolbg1,vcache2b,vcache4b,vcache8b,bg3highst,colormodeofs
EXTSYM drawline16b,curypos,mosaicon,mosaicsz,cachetile2b,cachetile4b,cachetile8b
EXTSYM vram,cachetile2b16x16,cachetile4b16x16,cachetile8b16x16

SECTION .bss
NEWSYM bgcoloradder, resb 1
NEWSYM res512switch, resb 1

SECTION .text

;*******************************************************
; DrawLine                        Draws the current line
;*******************************************************
; use curypos+bg1scroly for y location and bg1scrolx for x location
; use bg1ptr(b,c,d) for the pointer to the tile number contents
; use bg1objptr for the pointer to the object tile contents

%macro decideonmode 0
    cmp bl,2
    je .yes4bit
    cmp bl,1
    je .yes2bit
    mov byte[bshifter],6
    mov edx,[vcache8b]
    jmp .skipbits
.yes4bit
    mov byte[bshifter],2
    mov edx,[vcache4b]
    shl eax,1
    jmp .skipbits
.yes2bit
    mov byte[bshifter],0
    shl eax,2
    mov edx,[vcache2b]
.skipbits
%endmacro

SECTION .data
NEWSYM MosaicYAdder, dw 0,0,0,1,0,2,1,0,0,4,2,2,3,1,0,7

NEWSYM cwinptr,    dd winbgdata

SECTION .bss
NEWSYM pwinbgenab, resb 1
NEWSYM pwinbgtype, resd 1
NEWSYM winonbtype, resb 1
NEWSYM dualwinbg,  resb 1
NEWSYM pwinspenab, resb 1
NEWSYM pwinsptype, resd 1
NEWSYM winonstype, resb 1
NEWSYM dualwinsp,  resb 1
NEWSYM dwinptrproc, resd 1

SECTION .text

NEWSYM makewindow
    ; upon entry, al = win enable bits
    cmp byte[disableeffects],1
    je near .finishwin
    mov bl,al
    and bl,00001010b
    cmp bl,00001010b
    je near makedualwin
    cmp bl,0
    je near .finishwin
    mov byte[winon],1
    mov ebx,[winl1]
    ; check if data matches previous sprite data
    cmp al,[pwinspenab]
    jne .skipsprcheck
    cmp ebx,[pwinsptype]
    jne .skipsprcheck
    mov dword[cwinptr],winspdata+16
    mov al,[winonstype]
    mov [winon],al
    ret
.skipsprcheck
    ; check if data matches previous data
    cmp al,[pwinbgenab]
    jne .skipenab
    cmp ebx,[pwinbgtype]
    jne .skipenab2
    mov dword[cwinptr],winbgdata+16
    mov al,[winonbtype]
    mov [winon],al
    ret
.skipenab
    mov [pwinbgenab],al
    mov ebx,[winl1]
.skipenab2
    mov [pwinbgtype],ebx
    mov dl,[winl1]
    mov dh,[winr1]
    test al,00000010b
    jnz .win1
    mov dl,[winl2]
    mov dh,[winr2]
    shr al,2
.win1
    test al,01h
    jnz near .outside
    cmp dl,254
    je .clipped
    cmp dl,dh
    jb .clip
.clipped
    mov byte[winon],0
    mov byte[winonbtype],0
    ret
.clip
    mov edi,winbgdata+16
    xor eax,eax
    ; start drawing 1's from 0 to left
    cmp dl,0
    je .nextdot2
.nextdot
    mov byte[edi+eax],0
    inc al
    cmp al,dl
    jb .nextdot         ; blah
.nextdot2
    mov byte[edi+eax],1
    inc al
    cmp al,dh
    jb .nextdot2
    mov byte[edi+eax],1
    cmp dh,255
    je .nextdot4
    ; start drawing 1's from right to 255
.nextdot3
    mov byte[edi+eax],0
    inc al
    jnz .nextdot3
.nextdot4
    mov byte[winon],1
    mov byte[winonbtype],1
    mov dword[cwinptr],winbgdata+16
    ret
.outside
    cmp dl,dh
    jb .clip2
    mov byte[winon],0FFh
    mov byte[winonbtype],0FFh
    mov dword[cwinptr],winbgdata+16
    ret
.clip2
    cmp dl,1
    ja .nooutclip
    cmp dh,254
    jae near .clipped
.nooutclip
    mov edi,winbgdata+16
    xor eax,eax
    ; start drawing 1's from 0 to left
.nextdoti
    mov byte[edi+eax],1
    inc al
    cmp al,dl
    jb .nextdoti
.nextdot2i
    mov byte[edi+eax],0
    inc al
    cmp al,dh
    jb .nextdot2i
    mov byte[edi+eax],0
    cmp al,255
    je .nextdot4i
    inc al
    ; start drawing 1's from right to 255
.nextdot3i
    mov byte[edi+eax],1
    inc al
    jnz .nextdot3i
.nextdot4i
    mov byte[winon],1
    mov byte[winonbtype],1
    mov dword[cwinptr],winbgdata+16
    ret
.finishwin
    ret

NEWSYM makedualwin
    mov ecx,ebp
    shl cl,1
    mov dl,[winlogica]
    shr dl,cl
    and dl,03h
    mov cl,dl
    mov byte[winon],1
    mov ebx,[winl1]
    ; check if data matches previous sprite data
    cmp cl,[dualwinsp]
    jne .skipsprcheck
    cmp al,[pwinspenab]
    jne .skipsprcheck
    cmp ebx,[pwinsptype]
    jne .skipsprcheck
    mov dword[cwinptr],winspdata+16
    mov al,[winonstype]
    mov [winon],al
    ret
.skipsprcheck
    ; check if data matches previous data
    cmp cl,[dualwinbg]
    jne .skipenab3
    cmp al,[pwinbgenab]
    jne .skipenab
    cmp ebx,[pwinbgtype]
    jne .skipenab2
    mov dword[cwinptr],winbgdata+16
    mov al,[winonbtype]
    mov [winon],al
    ret
.skipenab3
    mov [dualwinbg],cl
.skipenab
    mov [pwinbgenab],al
    mov ebx,[winl1]
.skipenab2
    mov [pwinbgtype],ebx
    mov dword[dwinptrproc],winbgdata+16
    mov dword[cwinptr],winbgdata+16
    mov byte[winon],1
    mov byte[winonbtype],1

NEWSYM dualstartprocess

    mov dl,[winl1]
    mov dh,[winr1]

    push eax
    push ecx
    test al,01h
    jnz near .outside
    cmp dl,254
    je .clipped
    cmp dl,dh
    jb .clip
.clipped
    mov edi,[dwinptrproc]
    xor eax,eax
    mov ecx,64
    rep stosd
    jmp .donextwin
.clip
    mov edi,[dwinptrproc]
    xor eax,eax
    ; start drawing 1's from 0 to left
    cmp dl,0
    je .nextdot2
.nextdot
    mov byte[edi+eax],0
    inc al
    cmp al,dl
    jbe .nextdot
.nextdot2
    mov byte[edi+eax],1
    inc al
    cmp al,dh
    jb .nextdot2
    mov byte[edi+eax],1
    cmp dh,255
    je .nextdot4
    ; start drawing 1's from right to 255
.nextdot3
    mov byte[edi+eax],0
    inc al
    jnz .nextdot3
.nextdot4
    jmp .donextwin
.outside
    cmp dl,dh
    jb .clip2
    mov edi,[dwinptrproc]
    mov eax,01010101h
    mov ecx,64
    rep stosd
    jmp .donextwin
.clip2
    cmp dl,1
    ja .nooutclip
    cmp dh,254
    jae near .clipped
.nooutclip
    mov edi,[dwinptrproc]
    xor eax,eax
    ; start drawing 1's from 0 to left
.nextdoti
    mov byte[edi+eax],1
    inc al
    cmp al,dl
    jb .nextdoti
.nextdot2i
    mov byte[edi+eax],0
    inc al
    cmp al,dh
    jb .nextdot2i
    mov byte[edi+eax],0
    cmp al,255
    je .nextdot4i
    inc al
    ; start drawing 1's from right to 255
.nextdot3i
    mov byte[edi+eax],1
    inc al
    jnz .nextdot3i
.nextdot4i
.donextwin
    pop ecx
    pop eax
    cmp cl,0
    je near dualwinor
    cmp cl,2
    je near dualwinxor
    cmp cl,3
    je near dualwinxnor

NEWSYM dualwinand
    mov dl,[winl2]
    mov dh,[winr2]
    test al,04h
    jnz near .outside
    cmp dl,254
    je .clipped
    cmp dl,dh
    jb .clip
.clipped
    mov edi,[dwinptrproc]
    xor eax,eax
    mov ecx,64
    rep stosd
    jmp .donextwin
.clip
    mov edi,[dwinptrproc]
    xor eax,eax
    ; start drawing 1's from 0 to left
    cmp dl,0
    je .nextdot2
.nextdot
    mov byte[edi+eax],0
    inc al
    cmp al,dl
    jbe .nextdot
.nextdot2
    and byte[edi+eax],1
    inc al
    cmp al,dh
    jb .nextdot2
    and byte[edi+eax],1
    cmp dh,255
    je .nextdot4
    ; start drawing 1's from right to 255
.nextdot3
    mov byte[edi+eax],0
    inc al
    jnz .nextdot3
.nextdot4
    jmp .donextwin
.outside
    cmp dl,dh
    jb .clip2
    jmp .donextwin
.clip2
    cmp dl,1
    ja .nooutclip
    cmp dh,254
    jae near .clipped
.nooutclip
    mov edi,[dwinptrproc]
    xor eax,eax
    ; start drawing 1's from 0 to left
.nextdoti
    and byte[edi+eax],1
    inc al
    cmp al,dl
    jb .nextdoti
.nextdot2i
    mov byte[edi+eax],0
    inc al
    cmp al,dh
    jb .nextdot2i
    mov byte[edi+eax],0
    cmp al,255
    je .nextdot4i
    inc al
    ; start drawing 1's from right to 255
.nextdot3i
    and byte[edi+eax],1
    inc al
    jnz .nextdot3i
.nextdot4i
.donextwin
    ret

NEWSYM dualwinor
    mov dl,[winl2]
    mov dh,[winr2]
    test al,04h
    jnz near .outside
    cmp dl,254
    je .clipped
    cmp dl,dh
    jb .clip
.clipped
    jmp .donextwin
.clip
    mov edi,[dwinptrproc]
    xor eax,eax
    ; start drawing 1's from 0 to left
    cmp dl,0
    je .nextdot2
    mov al,dl
    inc al
.nextdot2
    mov byte[edi+eax],1
    inc al
    cmp al,dh
    jb .nextdot2
    mov byte[edi+eax],1
    jmp .donextwin
.outside
    cmp dl,dh
    jb .clip2
    mov edi,[dwinptrproc]
    mov eax,01010101h
    mov ecx,64
    rep stosd
    jmp .donextwin
.clip2
    cmp dl,1
    ja .nooutclip
    cmp dh,254
    jae near .clipped
.nooutclip
    mov edi,[dwinptrproc]
    xor eax,eax
    ; start drawing 1's from 0 to left
.nextdoti
    mov byte[edi+eax],1
    inc al
    cmp al,dl
    jb .nextdoti
    mov al,dh
    cmp al,255
    je .nextdot4i
    inc al
    ; start drawing 1's from right to 255
.nextdot3i
    mov byte[edi+eax],1
    inc al
    jnz .nextdot3i
.nextdot4i
.donextwin
    ret

NEWSYM dualwinxor
    mov dl,[winl2]
    mov dh,[winr2]
    test al,04h
    jnz near .outside
    cmp dl,254
    je .clipped
    cmp dl,dh
    jb .clip
.clipped
    jmp .donextwin
.clip
    mov edi,[dwinptrproc]
    xor eax,eax
    ; start drawing 1's from 0 to left
    cmp dl,0
    je .nextdot2
    mov al,dl
    inc al
.nextdot2
    xor byte[edi+eax],1
    inc al
    cmp al,dh
    jb .nextdot2
    xor byte[edi+eax],1
    jmp .donextwin
.outside
    cmp dl,dh
    jb .clip2
    mov edi,[dwinptrproc]
    mov ecx,64
.loopxor
    xor dword[edi],01010101h
    add edi,4
    dec ecx
    jnz .loopxor
    jmp .donextwin
.clip2
    cmp dl,1
    ja .nooutclip
    cmp dh,254
    jae near .clipped
.nooutclip
    mov edi,[dwinptrproc]
    xor eax,eax
    ; start drawing 1's from 0 to left
.nextdoti
    xor byte[edi+eax],1
    inc al
    cmp al,dl
    jb .nextdoti
    mov al,dh
    cmp al,255
    je .nextdot4i
    inc al
    ; start drawing 1's from right to 255
.nextdot3i
    xor byte[edi+eax],1
    inc al
    jnz .nextdot3i
.nextdot4i
.donextwin
    ret

NEWSYM dualwinxnor
    mov dl,[winl2]
    mov dh,[winr2]
    test al,04h
    jnz near .outside
    cmp dl,254
    je .clipped
    cmp dl,dh
    jb .clip
.clipped
    jmp .donextwin
.clip
    mov edi,[dwinptrproc]
    xor eax,eax
    ; start drawing 1's from 0 to left
    cmp dl,0
    je .nextdot2
    mov al,dl
    inc al
.nextdot2
    xor byte[edi+eax],1
    inc al
    cmp al,dh
    jb .nextdot2
    xor byte[edi+eax],1
    jmp .donextwin
.outside
    cmp dl,dh
    jb .clip2
    mov edi,[dwinptrproc]
    mov ecx,64
.loopxor
    xor dword[edi],01010101h
    add edi,4
    dec ecx
    jnz .loopxor
    jmp .donextwin
.clip2
    cmp dl,1
    ja .nooutclip
    cmp dh,254
    jae near .clipped
.nooutclip
    mov edi,[dwinptrproc]
    xor eax,eax
    ; start drawing 1's from 0 to left
.nextdoti
    xor byte[edi+eax],1
    inc al
    cmp al,dl
    jb .nextdoti
    mov al,dh
    cmp al,255
    je .nextdot4i
    inc al
    ; start drawing 1's from right to 255
.nextdot3i
    xor byte[edi+eax],1
    inc al
    jnz .nextdot3i
.nextdot4i
.donextwin
    mov edi,[dwinptrproc]
    mov ecx,64
.loopxor2
    xor dword[edi],01010101h
    add edi,4
    dec ecx
    jnz .loopxor2
    ret

SECTION .bss
NEWSYM winonsp, resb 1
SECTION .text

NEWSYM makewindowsp
    mov al,[winobjen]
    mov byte[winonsp],0
    test dword[winenabm],1010h
    jz near .finishwin
    ; upon entry, al = win enable bits
    cmp byte[disableeffects],1
    je near .finishwin
    mov bl,al
    and bl,00001010b
    cmp bl,00001010b
    je near makedualwinsp
    cmp bl,0
    je near .finishwin
    mov byte[winonsp],1
    ; check if data matches previous data
    cmp al,[pwinspenab]
    jne .skipenab
    mov ebx,[winl1]
    cmp ebx,[pwinsptype]
    jne .skipenab2
    mov dword[cwinptr],winspdata+16
    mov al,[winonstype]
    mov [winonsp],al
    ret
.skipenab
    mov [pwinspenab],al
    mov ebx,[winl1]
.skipenab2
    mov [pwinsptype],ebx
    mov dl,[winl1]
    mov dh,[winr1]
    test al,00000010b
    jnz .win1
    mov dl,[winl2]
    mov dh,[winr2]
    shr al,2
.win1
    test al,01h
    jnz near .outside
    cmp dl,254
    je .clipped
    cmp dl,dh
    jb .clip
.clipped
    mov byte[winonsp],0
    mov byte[winonstype],0
    ret
.clip
    mov edi,winspdata+16
    xor eax,eax
    ; start drawing 1's from 0 to left
    cmp dl,0
    je .nextdot2
.nextdot
    mov byte[edi+eax],0
    inc al
    cmp al,dl
    jbe .nextdot
.nextdot2
    mov byte[edi+eax],1
    inc al
    cmp al,dh
    jb .nextdot2
    mov byte[edi+eax],1
    cmp dh,255
    je .nextdot4
    ; start drawing 1's from right to 255
.nextdot3
    mov byte[edi+eax],0
    inc al
    jnz .nextdot3
.nextdot4
    mov byte[winonsp],1
    mov byte[winonstype],1
    mov dword[cwinptr],winspdata+16
    ret
.outside
    cmp dl,dh
    jb .clip2
    mov byte[winonsp],0FFh
    mov byte[winonstype],0FFh
    mov dword[cwinptr],winspdata+16
    ret
.clip2
    cmp dl,1
    ja .nooutclip
    cmp dh,254
    jae near .clipped
.nooutclip
    mov edi,winspdata+16
    xor eax,eax
    ; start drawing 1's from 0 to left
.nextdoti
    mov byte[edi+eax],1
    inc al
    cmp al,dl
    jb .nextdoti
.nextdot2i
    mov byte[edi+eax],0
    inc al
    cmp al,dh
    jb .nextdot2i
    mov byte[edi+eax],0
    cmp al,255
    je .nextdot4i
    inc al
    ; start drawing 1's from right to 255
.nextdot3i
    mov byte[edi+eax],1
    inc al
    jnz .nextdot3i
.nextdot4i
    mov byte[winonsp],1
    mov byte[winonstype],1
    mov dword[cwinptr],winspdata+16
    ret
.finishwin
    ret

NEWSYM makedualwinsp
    mov ecx,ebp
    shl cl,1
    mov dl,[winlogicb]
    and dl,03h
    mov cl,dl
    mov byte[winonsp],1
    ; check if data matches previous data
    cmp cl,[dualwinsp]
    jne .skipenab3
    cmp al,[pwinspenab]
    jne .skipenab
    mov ebx,[winl1]
    cmp ebx,[pwinsptype]
    jne .skipenab2
    mov dword[cwinptr],winspdata+16
    mov al,[winonstype]
    mov [winonsp],al
    ret
.skipenab3
    mov [dualwinsp],cl
.skipenab
    mov [pwinspenab],al
    mov ebx,[winl1]
.skipenab2
    mov [pwinsptype],ebx
    mov dword[dwinptrproc],winspdata+16
    mov dword[cwinptr],winspdata+16
    mov byte[winonsp],1
    mov byte[winonstype],1
    jmp dualstartprocess

; window logic data
SECTION .bss
NEWSYM windowdata, resb 16
NEWSYM numwin, resb 1
NEWSYM multiwin, resb 1
NEWSYM multiclip, resb 1
NEWSYM multitype, resb 1
SECTION .text

;    jmp .finishwin
%macro procwindow 1
    cmp byte[disableeffects],1
    je near .finishwin
    mov al,%1
    test al,00001010b
    jz near .finishwin
    mov esi,windowdata
    mov bl,al
    mov byte[winon],1
    and bl,00001010b
    and al,00000101b
    mov byte[numwin],0
    cmp bl,00001010b
    je near .multiwin
    mov byte[multiwin],0
    test bl,00000010b
    jnz .win1
    mov cl,[winl2]
    mov ch,[winr2]
    shr al,2
    jmp .okaywin
.win1
    mov cl,[winl1]
    mov ch,[winr1]
    and al,01h
.okaywin
    cmp ch,255
    je .noinc
    inc ch
.noinc
    test al,01h
    jnz .wininside
    cmp cl,ch
    jae .noinsidemask
    mov [esi],cl
    mov byte[esi+1],01h
    mov [esi+2],ch
    mov byte[esi+3],0FFh
    mov byte[numwin],2
    jmp .finishwin
.noinsidemask
    mov byte[winon],0
    jmp .finishwin
.wininside
    cmp cl,ch
    ja .nooutsidemask
.nonotoutside
    cmp ch,254
    jb .skipnodraw
    cmp cl,1
    jbe .noinsidemask
.skipnodraw
    mov byte[esi],0
    mov byte[esi+1],01h
    mov [esi+2],cl
    mov byte[esi+3],0FFh
    mov [esi+4],ch
    mov byte[esi+5],01h
    mov byte[numwin],3
    jmp .finishwin
.nooutsidemask
    mov byte[esi],0
    mov byte[esi+1],01h
    mov byte[numwin],1
    jmp .finishwin
    ; **************
    ; *Multiwindows*
    ; **************
.multiwin
    mov byte[winon],0
    mov byte[multiwin],0
    mov [multiclip],al
    mov al,[winlogica]
    mov ecx,ebp
    shl ecx,1
    shr al,cl
    and al,3h
    mov [multitype],al
    mov cl,[winl1]
    mov ch,[winr1]
    mov esi,windowdata
    cmp ch,255
    je .noinc2
    inc ch
.noinc2
    test byte[multiclip],01h
    jnz .wininside2
    cmp cl,ch
    jae .nowina
    mov [esi],cl
    mov byte[esi+1],01h
    mov [esi+2],ch
    mov byte[esi+3],0FFh
    add esi,4
    mov byte[numwin],2
    jmp .secondwin
.nowina
    mov cl,[winl2]
    mov ch,[winr2]
    mov al,[multiclip]
    shr al,2
    jmp .okaywin
.wininside2
    cmp cl,ch
    ja .nooutsidemask2
    cmp ch,254
    jb .skipnodraw2
    cmp cl,1
    jbe .nooutsidemask2
.skipnodraw2
    mov byte[esi],0
    mov byte[esi+1],01h
    mov [esi+2],cl
    mov byte[esi+3],0FFh
    mov [esi+4],ch
    mov byte[esi+5],01h
    mov byte[numwin],3
    jmp .secondwin
.nooutsidemask2
    mov byte[esi],0
    mov byte[esi+1],01h
    mov byte[numwin],1
.secondwin
    mov byte[multiwin],1
    mov byte[winon],1
.finishwin
%endmacro

SECTION .bss
NEWSYM curbgnum, resb 1
SECTION .text

NEWSYM procbackgrnd
    mov esi,[colormodeofs]
    mov bl,[esi+ebp]
    cmp bl,0
    je near .noback
    mov al,[curbgnum]
    mov ah,al
    test byte[scrndis],al
    jnz near .noback
    test [scrnon],ax
    jz near .noback
    push ebp
    shl ebp,6
    mov edi,cachebg1
    add edi,ebp
    pop ebp
    cmp bl,[curcolbg1+ebp]
    je .skipclearcache
    mov [curcolbg1+ebp],bl
    mov ax,[bg1ptr+ebp*2]
    mov [curbgofs1+ebp*2],ax
    call fillwithnothing
.skipclearcache
    xor eax,eax
    mov [curcolor],bl
    mov ax,[bg1objptr+ebp*2]
    decideonmode
    add edx,eax
    xor eax,eax
    mov [tempcach],edx
    xor edx,edx
    mov ax,[bg1objptr+ebp*2]
    mov [curtileptr],ax
    mov ax,[bg1ptr+ebp*2]
    mov [bgptr],ax
    cmp ax,[curbgofs1+ebp*2]
    je .skipclearcacheb
    mov [curbgofs1+ebp*2],ax
    call fillwithnothing
.skipclearcacheb
    mov ax,[bg1ptrb+ebp*2]
    mov [bgptrb],ax
    mov ax,[bg1ptrc+ebp*2]
    mov [bgptrc],ax
    mov ax,[bg1ptrd+ebp*2]
    mov [bgptrd],ax
    mov bl,[curbgnum]
    mov ax,[curypos]

    mov byte[curmosaicsz],1
    test byte[mosaicon],bl
    jz .nomos
    mov bl,[mosaicsz]
    cmp bl,0
    je .nomos
    inc bl
    mov [curmosaicsz],bl
    xor edx,edx
    xor bh,bh
    div bx
    xor edx,edx
    mul bx
    xor edx,edx
    mov dl,[mosaicsz]
    add ax,[MosaicYAdder+edx*2]
.nomos

    add ax,[bg1scroly+ebp*2]
    mov dx,[bg1scrolx+ebp*2]
    mov cl,[curbgnum]
    test byte[bgtilesz],cl
    jnz .16x16
    call proc8x8
    mov [bg1vbufloc+ebp*4],esi
    mov [bg1tdatloc+ebp*4],edi
    mov [bg1tdabloc+ebp*4],edx
    mov [bg1cachloc+ebp*4],ebx
    mov [bg1yaddval+ebp*4],ecx
    mov [bg1xposloc+ebp*4],eax
    ret
.16x16
    call proc16x16
    mov [bg1vbufloc+ebp*4],esi
    mov [bg1tdatloc+ebp*4],edi
    mov [bg1tdabloc+ebp*4],edx
    mov [bg1cachloc+ebp*4],ebx
    mov [bg1yaddval+ebp*4],ecx
    mov [bg1xposloc+ebp*4],eax
.noback
    ret

SECTION .bss
NEWSYM nextprimode, resb 1
NEWSYM cursprloc,   resd 1
NEWSYM curcolor,    resb 1
NEWSYM curtileptr,  resw 1
; esi = pointer to video buffer
; edi = pointer to tile data
; ebx = cached memory
; al = current x position
NEWSYM bg1vbufloc,  resd 1
NEWSYM bg2vbufloc,  resd 1
NEWSYM bg3vbufloc,  resd 1
NEWSYM bg4vbufloc,  resd 1
NEWSYM bg1tdatloc,  resd 1
NEWSYM bg2tdatloc,  resd 1
NEWSYM bg3tdatloc,  resd 1
NEWSYM bg4tdatloc,  resd 1
NEWSYM bg1tdabloc,  resd 1
NEWSYM bg2tdabloc,  resd 1
NEWSYM bg3tdabloc,  resd 1
NEWSYM bg4tdabloc,  resd 1
NEWSYM bg1cachloc,  resd 1
NEWSYM bg2cachloc,  resd 1
NEWSYM bg3cachloc,  resd 1
NEWSYM bg4cachloc,  resd 1
NEWSYM bg1yaddval,  resd 1
NEWSYM bg2yaddval,  resd 1
NEWSYM bg3yaddval,  resd 1
NEWSYM bg4yaddval,  resd 1
NEWSYM bg1xposloc,  resd 1
NEWSYM bg2xposloc,  resd 1
NEWSYM bg3xposloc,  resd 1
NEWSYM bg4xposloc,  resd 1
NEWSYM alreadydrawn, resb 1

SECTION .text

NEWSYM fillwithnothing
    push edi
    xor eax,eax
    mov ecx,16
.loop
    mov [edi],eax
    add edi,4
    dec ecx
    jnz .loop
    pop edi
    ret

SECTION .bss
NEWSYM bg3draw, resb 1
NEWSYM maxbr,   resb 1
SECTION .text

ALIGN32
SECTION .bss
NEWSYM bg3high2, resd 1
NEWSYM cwinenabm, resd 1
SECTION .text

NEWSYM drawline
    mov al,[winenabs]
    mov [cwinenabm],al

    mov byte[bg3high2],0
    cmp byte[bgmode],1
    jne .nohigh
    mov al,[bg3highst]
    mov [bg3high2],al
.nohigh
    jmp drawline16b

ALIGN32
SECTION .bss
NEWSYM tempbuffer, resd 33
NEWSYM currentobjptr, resd 1
NEWSYM curmosaicsz,   resd 1
NEWSYM extbgdone, resb 1
SECTION .data
NEWSYM csprbit, db 1
NEWSYM csprprlft, db 0

SECTION .text
;*******************************************************
; Processes & Draws 8x8 tiles in 2, 4, & 8 bit mode
;*******************************************************
NEWSYM proc8x8
    cmp byte[bgmode],5
    je near proc16x8
    ; ax = # of rows down
    mov ebx,eax
    shr eax,3
    and eax,63
    and ebx,07h
    cmp byte[edi+eax],0
    jne .nocachereq
;.docache
;    cmp byte[ccud],0
;    jne .nocachereq
    mov byte[edi+eax],1
    cmp byte[curcolor],2
    jne .no4b
    ; cache 4-bit
    call cachetile4b
    jmp .nocachereq
.no4b
    cmp byte[curcolor],1
    je .2b
    ; cache 8-bit
    call cachetile8b
    jmp .nocachereq
.2b
    ; cache 2-bit
    call cachetile2b
.nocachereq
    test edx,0100h
    jz .tilexa
    test al,20h
    jz .tileya
    ; bgptrd/bgptrc
    mov ecx,[bgptrd]
    mov [bgptrx1],ecx
    mov ecx,[bgptrc]
    mov [bgptrx2],ecx
    jmp .skiptile
.tileya
    ; bgptrb/bgptra
    mov ecx,[bgptrb]
    mov [bgptrx1],ecx
    mov ecx,[bgptr]
    mov [bgptrx2],ecx
    jmp .skiptile
.tilexa
    test al,20h
    jz .tileya2
    ; bgptrc/bgptrd
    mov ecx,[bgptrc]
    mov [bgptrx1],ecx
    mov ecx,[bgptrd]
    mov [bgptrx2],ecx
    jmp .skiptile
.tileya2
    ; bgptra/bgptrb
    mov ecx,[bgptr]
    mov [bgptrx1],ecx
    mov ecx,[bgptrb]
    mov [bgptrx2],ecx
.skiptile
    ; set up edi & yadder to point to tile data
    shl ebx,3
    mov [yadder],ebx
    and al,1Fh
    mov edi,[vram]
    mov ebx,eax
    shl ebx,6
    mov eax,[bgptrx1]
    add edi,ebx
    mov [temptile],edi
    add edi,eax
    ; dx = # of columns right
    ; cx = bgxlim
    mov eax,edx
    shr edx,3
    mov bl,[curypos]
    and edx,1Fh
    mov [temp],dl
    and eax,07h
    add dl,dl
    add edi,edx

    mov esi,eax
    mov ebx,[tempcach]
    mov edx,[temptile]
    mov eax,[bgptrx2]
    and eax,0FFFFh
    add edx,eax
    mov al,[temp]
    mov ecx,[yadder]
    mov ah,[bshifter]
    ; fill up tempbuffer with pointer #s that point to cached video mem
    ; to calculate pointer, get first byte
    ; esi = pointer to video buffer
    ; edi = pointer to tile data
    ; ebx = cached memory
    ; ecx = y adder
    ; edx = secondary tile pointer
    ; al = current x position
    ret

NEWSYM proc16x8
    ; ax = # of rows down
    mov ebx,eax
    shr eax,3
    and ebx,07h
    and eax,63
    cmp byte[edi+eax],0
    jne .nocachereq
;    cmp byte[ccud],0
;    jne .nocachereq
    mov byte[edi+eax],1
    cmp byte[curcolor],2
    jne .no4b
    ; cache 4-bit
    call cachetile4b16x16
    jmp .nocachereq
.no4b
    cmp byte[curcolor],1
    je .2b
    ; cache 8-bit
    call cachetile8b16x16
    jmp .nocachereq
.2b
    ; cache 2-bit
    call cachetile2b16x16
.nocachereq
    test edx,0100h
    jz .tilexa
    test al,20h
    jz .tileya
    ; bgptrd/bgptrc
    mov ecx,[bgptrd]
    mov [bgptrx1],ecx
    mov ecx,[bgptrc]
    mov [bgptrx2],ecx
    jmp .skiptile
.tileya
    ; bgptrb/bgptra
    mov ecx,[bgptrb]
    mov [bgptrx1],ecx
    mov ecx,[bgptr]
    mov [bgptrx2],ecx
    jmp .skiptile
.tilexa
    test al,20h
    jz .tileya2
    ; bgptrc/bgptrd
    mov ecx,[bgptrc]
    mov [bgptrx1],ecx
    mov ecx,[bgptrd]
    mov [bgptrx2],ecx
    jmp .skiptile
.tileya2
    ; bgptra/bgptrb
    mov ecx,[bgptr]
    mov [bgptrx1],ecx
    mov ecx,[bgptrb]
    mov [bgptrx2],ecx
.skiptile
    ; set up edi & yadder to point to tile data
    shl ebx,3
    mov [yadder],ebx
    and al,1Fh
    mov edi,[vram]
    mov ebx,eax
    shl ebx,6
    mov eax,[bgptrx1]
    add edi,ebx
    mov [temptile],edi
    add edi,eax
    ; dx = # of columns right
    ; cx = bgxlim
    mov eax,edx
    shr edx,3
    mov bl,[curypos]
    and edx,1Fh
    mov [temp],dl
    and eax,07h
    add dl,dl
    add edi,edx

    mov esi,eax
    mov ebx,[tempcach]
    mov edx,[temptile]
    mov eax,[bgptrx2]
    and eax,0FFFFh
    add edx,eax
    mov al,[temp]
    mov ecx,[yadder]
    mov ah,[bshifter]
    ; fill up tempbuffer with pointer #s that point to cached video mem
    ; to calculate pointer, get first byte
    ; esi = pointer to video buffer
    ; edi = pointer to tile data
    ; ebx = cached memory
    ; ecx = y adder
    ; edx = secondary tile pointer
    ; al = current x position
    ret

SECTION .bss
NEWSYM drawn, resb 1
NEWSYM curbgpr, resb 1    ; 00h = low priority, 20h = high priority
SECTION .text

%macro drawpixel8b8x8 3
    or %1,%1
    jz %2
    add %1,dh
    mov [esi+%3],%1
%2
%endmacro

%macro drawpixel8b8x8win 3
    or %1,%1
    jz %2
    test byte[ebp+%3],0FFh
    jnz %2
    add %1,dh
    mov [esi+%3],%1
%2
%endmacro

ALIGN32
SECTION .bss
NEWSYM winptrref, resd 1
NEWSYM hirestiledat, resb 256
NEWSYM yadder,     resd 1
NEWSYM yrevadder,  resd 1
NEWSYM tempcach,   resd 1        ; points to cached memory
NEWSYM temptile,   resd 1        ; points to the secondary video pointer
NEWSYM bgptr,      resd 1
NEWSYM bgptrb,     resd 1
NEWSYM bgptrc,     resd 1
NEWSYM bgptrd,     resd 1
NEWSYM bgptrx1,    resd 1
NEWSYM bgptrx2,    resd 1
NEWSYM curvidoffset, resd 1
NEWSYM winon,      resd 1
NEWSYM bgofwptr,   resd 1
NEWSYM bgsubby,    resd 1

SECTION .text
;*******************************************************
; Processes & Draws 16x16 tiles in 2, 4, & 8 bit mode
;*******************************************************

NEWSYM proc16x16
    ; ax = # of rows down
    xor ebx,ebx
    mov ebx,eax
    and ebx,07h
    mov byte[a16x16yinc],0
    test eax,08h
    jz .noincb
    mov byte[a16x16yinc],1
.noincb
    shr eax,4
    and eax,63
    cmp byte[edi+eax],0
    jne .nocachereq
    mov byte[edi+eax],1
    cmp byte[curcolor],2
    jne .no4b
    ; cache 4-bit
    call cachetile4b16x16
    jmp .nocachereq
.no4b
    cmp byte[curcolor],1
    je .2b
    ; cache 8-bit
    call cachetile8b16x16
    jmp .nocachereq
.2b
    ; cache 2-bit
    call cachetile2b16x16
.nocachereq
    test edx,0200h
    jz .tilexa
    test eax,20h
    jz .tileya
    ; bgptrd/bgptrc
    mov ecx,[bgptrd]
    mov [bgptrx1],ecx
    mov ecx,[bgptrc]
    mov [bgptrx2],ecx
    jmp .skiptile
.tileya
    ; bgptrb/bgptra
    mov ecx,[bgptrb]
    mov [bgptrx1],ecx
    mov ecx,[bgptr]
    mov [bgptrx2],ecx
    jmp .skiptile
.tilexa
    test ax,20h
    jz .tileya2
    ; bgptrc/bgptrd
    mov ecx,[bgptrc]
    mov [bgptrx1],ecx
    mov ecx,[bgptrd]
    mov [bgptrx2],ecx
    jmp .skiptile
.tileya2
    ; bgptra/bgptrb
    mov ecx,[bgptr]
    mov [bgptrx1],ecx
    mov ecx,[bgptrb]
    mov [bgptrx2],ecx
.skiptile
    and eax,1Fh
    shl ebx,3
    mov [yadder],ebx
    ; set up edi to point to tile data
    mov edi,[vram]
    mov ebx,eax
    shl ebx,6
    mov ax,[bgptrx1]
    add edi,ebx
    mov [temptile],edi
    add edi,eax
    ; dx = # of columns right
    ; cx = bgxlim
    mov eax,edx
    mov byte[a16x16xinc],0
    test edx,08h
    jz .noincd
    mov byte[a16x16xinc],1
.noincd
    shr edx,4
    and edx,1Fh
    mov [temp],dl
    and eax,07h
    shl dl,1
    xor ebx,ebx
    add edi,edx

    mov esi,eax
    mov ebx,[tempcach]
    xor eax,eax
    mov edx,[temptile]
    mov ax,[bgptrx2]
    add edx,eax
    mov ecx,[yadder]
    mov eax,[temp]
    ; fill up tempbuffer with pointer #s that point to cached video mem
    ; to calculate pointer, get first byte
    ; esi = pointer to video buffer
    ; edi = pointer to tile data
    ; ebx = cached memory
    ; ecx = y adder
    ; edx = secondary tile pointer
    ; al = current x position
    ret

SECTION .bss
NEWSYM temp,       resb 1
NEWSYM bshifter,   resb 1
NEWSYM a16x16xinc, resb 1
NEWSYM a16x16yinc, resb 1
SECTION .text
