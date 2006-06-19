; Stuff that looked like too much trouble to port

%include "macros.mac"

EXTSYM snesmmap, snesmap2, memtabler8, regaccessbankr8, dmadata
EXTSYM initaddrl, spcPCRam, UpdateDPage, pdh, numinst
EXTSYM xp, xpb, xpc, curcyc, Curtableaddr, splitflags, execsingle, joinflags

;; Wrapper for calls to routines in memtabler8

NEWSYM memtabler8_wrapper
        push    ebp
        mov     ebp, esp
        push    ebx
        mov     bl, BYTE [ebp+8]
        mov     ecx, DWORD [ebp+12]
        xor     eax, eax
        mov     al, bl
        call    DWORD [memtabler8+eax*4]
        and     eax, 255
        pop     ebx
        pop     ebp
        ret
 
		
;*******************************************************
; Execute Next Opcode
;*******************************************************

NEWSYM execnextop
    xor eax,eax
    xor ebx,ebx
    xor ecx,ecx
    xor edx,edx
    mov bl,[xpb]
    mov ax,[xpc]
    test ax,8000h
    jz .loweraddr
    mov esi,[snesmmap+ebx*4]
    jmp .skiplower
.loweraddr
    cmp ax,4300h
    jb .lower
    cmp dword[memtabler8+ebx*4],regaccessbankr8
    je .dma
.lower
    mov esi,[snesmap2+ebx*4]
    jmp .skiplower
.dma
    mov esi,dmadata-4300h
.skiplower
    mov [initaddrl],esi
    add esi,eax                 ; add program counter to address
    mov ebp,[spcPCRam]
    mov dl,[xp]                 ; set flags
    mov dh,[curcyc]             ; set cycles
    mov edi,[Curtableaddr]
    call splitflags
    call execsingle
    call joinflags
    call UpdateDPage
    ; execute
    ; copy back data
    mov [spcPCRam],ebp
    mov [Curtableaddr],edi
    mov [xp],dl
    mov dh,[pdh]
    mov [curcyc],dh

    mov eax,[initaddrl]
    sub esi,eax                 ; subtract program counter by address
    mov [xpc],si
    inc dword[numinst]
    ret
