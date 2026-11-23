        .386p
        .model  flat

; Big Red Racing DOS patch (Phase 3b): the pieces that need exact registers.
; Built by "python3 brr.py dos-build" with Open Watcom's assembler. Names follow Watcom's
; register convention: C functions end in "_", C variables start with "_".

        public  brr_hook_rotation_, brr_rotation_trampoline_, brr_call_rotation_original_
        public  brr_cam4_, brr_cap_, brr_approach_
        public  brr_menu_tick_, brr_hook_menu_mouse_, brr_hook_hotspot_draw_, brr_hook_label_draw_
        public  brr_sound_sprite_down_, brr_sound_sprite_up_, brr_sound_sprite_up2_
        extrn   nav_before_tick_ : near
        extrn   nav_after_tick_ : near
        extrn   nav_before_draw_ : near
        extrn   nav_after_draw_ : near
        extrn   nav_sprite_frame_ : near
        extrn   nav_label_font_ : near
        extrn   game_text_draw_ : near
        extrn   brr_dos_menu_mouse_ : near
        extrn   game_menu_hotspots_tick_ : near
        extrn   game_menu_hotspot_draw_ : near
        extrn   game_sprite_draw_ : near
        extrn   brr_frame_start_ : near
        public  _brr_options, _brr_input, _brr_input_count, _brr_tag
        extrn   brr_rotation_fine_ : near
        extrn   _brr_fine_active : dword
        extrn   _game_frame_ticks : byte

; Filled in by dos-patch: the options (brrdos.h, OPT_...), the scripted input of the
; logging build (key, first tick, last tick), and the version text shown on the menu.
; Read-only at run time, after our code (the build places CONST straight after it).
CONST   segment dword public use32 'DATA'
_brr_options            dd      14 dup (0)
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
; Without the camera fix (option camera, the first dword of _brr_options) it stores it
; every frame, as the original.
brr_cam4_:
        mov     ax, word ptr [esi + 10h]
        cmp     dword ptr _brr_options, 0
        je      cam4_store
        cmp     byte ptr _game_frame_ticks, 0
        je      cam4_done
cam4_store:
        mov     word ptr [esi + 46h], ax
cam4_done:
        ret

; approach_clamped (0x15862): MOV AL,[frame_ticks] becomes a call here. With the camera fix,
; one step per tick (Phase 3a); without it, the frame's ticks, as the original. IMUL follows,
; so the flags don't matter.
brr_approach_:
        mov     al, 1
        cmp     dword ptr _brr_options, 0
        jne     approach_done
        mov     al, byte ptr _game_frame_ticks
approach_done:
        ret

; run_screen (0x2C5BF): CMP AL,35; JBE; MOV byte [frame_ticks],35 (11 bytes) become a
; call here and NOPs: brr_frame_start applies the cap and tracks the machine's speed.
; Everything is kept as it was, EAX included.
brr_cap_:
        push    eax
        call    brr_frame_start_
        pop     eax
        ret

; The joystick menu navigation (../dropin/menunav.c, navdos.c; option menu_pad).
; run_screen's per-tick CALL menu_hotspots_tick (0x2C64C): the navigation before and after
; it. DL is run_screen's tick counter: every register is kept.
brr_menu_tick_:
        pushad
        call    nav_before_tick_
        popad
        call    game_menu_hotspots_tick_
        pushad
        call    nav_after_tick_
        popad
        ret

; menu_mouse_update's end (0x2517B: POP EDI; POP ECX; POP EBX; RET), reached by a JMP:
; the navigation's say on the menu's pointer and buttons, then those instructions.
brr_hook_menu_mouse_:
        pushad
        call    brr_dos_menu_mouse_
        popad
        pop     edi
        pop     ecx
        pop     ebx
        ret

; menu_draw_group's CALL menu_hotspot_draw (0x259BF), EAX the hotspot: the navigation's notes
; either side of it. Every register is kept.
brr_hook_hotspot_draw_:
        push    eax
        pushad
        call    nav_before_draw_
        popad
        call    game_menu_hotspot_draw_
        pushad
        mov     eax, [esp + 32]
        call    nav_after_draw_
        popad
        add     esp, 4
        ret

; joystick_options_draw_type's CALL text_draw (0x32584; EAX the text, EDX 0, EBX 0x7FFF, text_x
; and text_y set): the label's font from its line (nav_label_font), then text_draw.
brr_hook_label_draw_:
        pushad
        call    nav_label_font_
        popad
        jmp     game_text_draw_

; sound_options_draw's CALL sprite_draw of a button (EDX its frame, EBX x, ECX y): the frame
; nav_sprite_frame gives (its step of the screen's animation kept in the joystick's
; selection), then sprite_draw. Each entry is how the button's frame follows the step.
brr_sound_sprite_down_:
        push    -1
        jmp     short sound_sprite
brr_sound_sprite_up_:
        push    1
        jmp     short sound_sprite
brr_sound_sprite_up2_:
        push    2
sound_sprite:
        pushad
        movsx   eax, dx
        mov     edx, ebx
        mov     ebx, [esp + 24]
        mov     ecx, [esp + 32]
        call    nav_sprite_frame_
        mov     [esp + 20], eax
        popad
        add     esp, 4
        jmp     game_sprite_draw_

        end
