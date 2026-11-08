        .386p
        .model  flat

; Big Red Racing DOS patch (Phase 3b): the pieces that need exact registers.
; Built by "python3 brr.py dos-build" with Open Watcom's assembler. Names follow Watcom's
; register convention: C functions end in "_", C variables start with "_".

        public  brr_hook_rotation_, brr_rotation_trampoline_, brr_call_rotation_original_
        public  brr_cam4_
        public  _brr_options, _brr_input, _brr_input_count, _brr_tag
        extrn   brr_rotation_fine_ : near
        extrn   _brr_fine_active : dword
        extrn   _game_frame_ticks : byte

; Filled in by dos-patch: the options (brrdos.h, OPT_...), the scripted input of the
; logging build (key, first tick, last tick), and the version text shown on the menu.
; Read-only at run time, after our code (the build places CONST straight after it).
CONST   segment dword public use32 'DATA'
_brr_options            dd      12 dup (0)
_brr_input              dd      24 dup (0)
_brr_input_count        dd      0
_brr_tag                db      32 dup (0)
CONST   ends

        .code

; rotation_matrix (0x59E20) is entered by a JMP to here. EAX pitch, EBX yaw, ECX roll
; (low 16 bits), EBP the 3x3 matrix to fill. During the race draw with fine angles on,
; brr_rotation_fine builds it (from a struct rot_args on the stack) and the registers
; are left as the original leaves them; otherwise the original runs.
brr_hook_rotation_:
        cmp     dword ptr _brr_fine_active, 0
        je      brr_rotation_trampoline_
        sub     esp, 32
        mov     [esp], eax
        mov     [esp + 4], ebx
        mov     [esp + 8], ecx
        mov     [esp + 12], ebp
        mov     eax, esp
        call    brr_rotation_fine_
        mov     eax, [esp + 16]
        mov     ebx, [esp + 20]
        mov     ecx, [esp + 24]
        mov     edx, [esp + 28]
        add     esp, 32
        ret

; The original's first instructions (the 5 bytes the JMP replaced), copied here from the
; player's own file by dos-patch, then a JMP back to the rest of the original.
brr_rotation_trampoline_:
        db      10 dup (0CCh)

; void brr_call_rotation_original(struct rot_args *a): the original routine, for --prove
brr_call_rotation_original_:
        push    ebx
        push    ecx
        push    edx
        push    esi
        push    edi
        push    ebp
        mov     esi, eax
        mov     ebp, [esi + 12]
        mov     ecx, [esi + 8]
        mov     ebx, [esi + 4]
        mov     eax, [esi]
        push    esi
        call    brr_rotation_trampoline_
        pop     esi
        mov     [esi + 16], eax
        mov     [esi + 20], ebx
        mov     [esi + 24], ecx
        mov     [esi + 28], edx
        pop     ebp
        pop     edi
        pop     esi
        pop     edx
        pop     ecx
        pop     ebx
        ret

; camera_mode4 (0x16CB4): the original stores the yaw as "previous" every frame, so the
; roll (worked out from the yaw's change per tick) is wrong on frames that ran no tick.
; Its three instructions become MOV word [ESI+24h],0 and a call here, which stores it
; only when a tick ran.
brr_cam4_:
        mov     ax, word ptr [esi + 10h]
        cmp     byte ptr _game_frame_ticks, 0
        je      cam4_done
        mov     word ptr [esi + 46h], ax
cam4_done:
        ret

        end
