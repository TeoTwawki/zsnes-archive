%macro spccycle 0
      push eax
      mov eax,[cycpblt]
      inc dword[spcCycle]
      add [cycpbl],eax
      pop eax
%endmacro

%macro spccycles 1
      push eax
      push edx
  %ifnidn %1, eax
      mov eax,%1
  %endif
      add [spcCycle],eax
      mul dword[cycpblt]
      add [cycpbl],eax
      pop edx
      pop eax
%endmacro
