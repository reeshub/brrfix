; Register-preserving entry points between the game's Watcom code and our C code.
; Watcom callers expect every register except EAX to survive a call; our C code
; (cdecl) only preserves EBX, ESI, EDI and EBP, so each thunk saves the rest.
; Open Watcom assembler (wasm); 386 instructions only.
        .386
        .model  flat

        extrn   _brr_timer_start:near
        extrn   _brr_timer_stop:near
        extrn   _brr_clock_read:near
        extrn   _brr_retrace:near
        extrn   _brr_before_tick:near
        extrn   _brr_after_tick:near
        extrn   _brr_before_draw:near
        extrn   _brr_after_draw:near
        extrn   _brr_after_camera_update:near
        extrn   _brr_rotation_fine:near
        extrn   _brr_timer_count:dword
        extrn   _brr_orig_race_tick:dword
        extrn   _brr_orig_race_draw:dword
        extrn   _brr_orig_playback_tick:dword
        extrn   _brr_orig_playback_draw:dword
        extrn   _brr_orig_camera_update:dword
        extrn   _brr_fine_angles_active:dword
        extrn   _brr_rotation_continue:dword
        extrn   _brr_game_atan:dword
        extrn   _brr_before_present_window:near
        extrn   _brr_display_choice:near
        extrn   _brr_before_joystick:near
        extrn   _brr_orig_joystick_to_keys:dword
        extrn   _brr_before_menu_hotspots:near
        extrn   _brr_after_menu_hotspots:near
        extrn   _brr_orig_menu_hotspots:dword
        extrn   _brr_present_gdi:near
        extrn   _brr_gdi_present:dword
        extrn   _brr_present_continue:dword
        extrn   _brr_display_windowed:dword
        extrn   _brr_display_fullscreen:dword
        extrn   _brr_display_prompt:dword
        extrn   _brr_orig_present_window:dword
        extrn   _brr_render_before_present:near
        extrn   _brr_render_after_present:near
        extrn   _brr_menu_mouse:near
        extrn   _brr_music_reassert:near
        extrn   _brr_render_mode:near
        extrn   _brr_dd_setup_continue:dword
        extrn   _brr_orig_music_check:dword
        extrn   _brr_render_view_continue:dword
        extrn   _brr_draw_vision:dword
        extrn   _brr_draw_texture:dword

        public  _brr_TIMER_Init, _brr_TIMER_Shutdown, _brr_TIMER_Handler
        public  _brr_hook_clock_read, _brr_hook_retrace, _brr_hook_race_tick
        public  _brr_hook_race_draw, _brr_hook_camera_update, _brr_hook_rotation
        public  _brr_hook_playback_tick, _brr_hook_playback_draw
        public  _brr_rotation_original, _brr_call_rotation_original
        public  _brr_call_game_atan, _brr_hook_cam4, _brr_hook_present_window
        public  _brr_hook_display_choice, _brr_hook_present, _brr_hook_joystick
        public  _brr_hook_menu_hotspots, _brr_hook_menu_mouse, _brr_hook_music_check
        public  _brr_hook_dd_setup, _brr_hook_render_view

        .code

; TIMER.DLL exports (named in dropin_build.py's linker directives).
; TIMER_Init_ takes hInstance in EAX.
_brr_TIMER_Init:
        push    ecx
        push    edx
        push    eax
        call    _brr_timer_start
        add     esp, 4
        pop     edx
        pop     ecx
        ret

_brr_TIMER_Shutdown:
        push    ecx
        push    edx
        call    _brr_timer_stop
        pop     edx
        pop     ecx
        ret

; The original's exported multimedia-timer callback (__stdcall, 5 arguments).
_brr_TIMER_Handler:
        lock inc dword ptr [_brr_timer_count]
        ret     20

; Patch targets.
_brr_hook_clock_read:                   ; returns the count in EAX (the game uses AX)
        push    ecx
        push    edx
        call    _brr_clock_read
        pop     edx
        pop     ecx
        ret

_brr_hook_retrace:                      ; reached by JMP from vga_wait_retrace
        pushad
        call    _brr_retrace
        popad
        ret

_brr_hook_race_tick:
        pushad
        call    _brr_before_tick
        popad
        call    dword ptr [_brr_orig_race_tick]
        pushad
        call    _brr_after_tick
        popad
        ret

_brr_hook_race_draw:
        pushad
        call    _brr_before_draw
        popad
        call    dword ptr [_brr_orig_race_draw]
        pushad
        call    _brr_after_draw
        popad
        ret

; "Load Recording" playback (screen 0x83): its tick and draw, wrapped like the race's
_brr_hook_playback_tick:
        pushad
        call    _brr_before_tick
        popad
        call    dword ptr [_brr_orig_playback_tick]
        pushad
        call    _brr_after_tick
        popad
        ret

_brr_hook_playback_draw:
        pushad
        call    _brr_before_draw
        popad
        call    dword ptr [_brr_orig_playback_draw]
        pushad
        call    _brr_after_draw
        popad
        ret

; render_view's call to camera_update (record in EAX; its result in EAX is used).
_brr_hook_camera_update:
        push    eax
        call    dword ptr [_brr_orig_camera_update]
        pushad
        push    dword ptr [esp + 32]
        call    _brr_after_camera_update
        add     esp, 4
        popad
        add     esp, 4
        ret

; FUN_00446670 (the 3D engine's rotation-matrix builder) is entered by a JMP to here.
; EAX pitch, EBX yaw, ECX roll (low 16 bits), EBP the 3x3 matrix to fill. During the race
; draw with fine angles on, our C version builds it; otherwise the original runs.
_brr_hook_rotation:
        cmp     dword ptr [_brr_fine_angles_active], 0
        je      _brr_rotation_original
        push    edi
        push    esi
        sub     esp, 16                 ; regs[4]: what the original leaves in EAX..EDX
        mov     esi, esp
        push    esi
        push    ebp
        push    ecx
        push    ebx
        push    eax
        call    _brr_rotation_fine
        add     esp, 20
        mov     eax, [esp]
        mov     ebx, [esp + 4]
        mov     ecx, [esp + 8]
        mov     edx, [esp + 12]
        add     esp, 16
        pop     esi
        pop     edi
        ret

; The original routine: its first three instructions (the five bytes the JMP replaced),
; then the rest of it in place.
_brr_rotation_original:
        push    edi
        push    esi
        shr     eax, 5
        jmp     dword ptr [_brr_rotation_continue]

; void brr_call_rotation_original(pitch, yaw, roll, int32_t *m, uint32_t *regs): for the proof
_brr_call_rotation_original:
        push    ebp
        push    ebx
        push    esi
        push    edi
        mov     eax, [esp + 20]
        mov     ebx, [esp + 24]
        mov     ecx, [esp + 28]
        mov     ebp, [esp + 32]
        call    _brr_rotation_original
        mov     esi, [esp + 36]
        mov     [esi], eax
        mov     [esi + 4], ebx
        mov     [esi + 8], ecx
        mov     [esi + 12], edx
        pop     edi
        pop     esi
        pop     ebx
        pop     ebp
        ret

; int brr_call_game_atan(int y, int x): the game's FUN_00412a20 (EAX = y, EDX = x)
_brr_call_game_atan:
        push    ebx
        mov     eax, [esp + 8]
        mov     edx, [esp + 12]
        call    dword ptr [_brr_game_atan]
        pop     ebx
        ret

; The present's call to the windowed blit (0x42CD2C): wait for the vertical blank first
; if vsync is on, then continue into the original routine (it returns to the present).
_brr_hook_present_window:
        pushad
        call    _brr_before_present_window
        popad
        jmp     dword ptr [_brr_orig_present_window]

; WinMain's colour-depth test (CMP EDI,8; JZ at 0x4010E9) calls here instead, with
; EDI = the desktop's bits per pixel x planes. Continue WinMain where brr_display_choice
; says (EAX is free on all three paths).
_brr_hook_display_choice:
        pushad
        push    edi
        call    _brr_display_choice
        add     esp, 4
        mov     [esp + 28], eax         ; into the saved EAX
        popad
        add     esp, 4                  ; drop the return address
        test    eax, eax
        jz      display_windowed
        dec     eax
        jz      display_fullscreen
        jmp     dword ptr [_brr_display_prompt]
display_windowed:
        jmp     dword ptr [_brr_display_windowed]
display_fullscreen:
        jmp     dword ptr [_brr_display_fullscreen]

; The present (0x42CE78) is entered by a JMP to here. First the menus' scaling may put our
; picture in its place (render.c); then, in a window on a desktop that isn't 256 colours,
; brr_present_gdi draws the frame (the present returns 1); otherwise the original runs,
; called as a subroutine (it takes no stack arguments and ends with a plain RET); finally
; the game's buffer is put back. EAX is the present's result throughout.
_brr_hook_present:
        pushad
        call    _brr_render_before_present
        popad
        cmp     dword ptr [_brr_gdi_present], 0
        je      present_game
        pushad
        call    _brr_present_gdi
        mov     [esp + 28], eax
        popad
        test    eax, eax
        jz      present_game
        mov     eax, 1
        jmp     present_done
present_game:
        call    present_original
present_done:
        pushad
        call    _brr_render_after_present
        popad
        ret
present_original:
        push    ebx
        push    ecx
        push    edx
        cmp     byte ptr ds:[52D720h], 0
        jmp     dword ptr [_brr_present_continue]

; Site menu_mouse (0x4274FC's mapping of the mouse into the menu): CX = the menu's x and
; BX = its y for the stores that follow; EAX is free there, EDX must survive.
_brr_hook_menu_mouse:
        push    edx
        call    _brr_menu_mouse
        pop     edx
        mov     ecx, eax
        shr     eax, 16
        mov     ebx, eax
        ret

; run_screen_frame's call to joystick_to_keys (0x428D1D): the pad's state goes into
; the game's joystick variables first, then the original runs.
_brr_hook_joystick:
        pushad
        call    _brr_before_joystick
        popad
        jmp     dword ptr [_brr_orig_joystick_to_keys]

; run_screen_frame's per-tick call to menu_hotspots_tick (0x428D31): our code before
; and after it (keys.c); the original runs in between, unchanged. DL is the frame's
; tick counter, so every register is kept.
_brr_hook_menu_hotspots:
        pushad
        call    _brr_before_menu_hotspots
        popad
        call    dword ptr [_brr_orig_menu_hotspots]
        pushad
        call    _brr_after_menu_hotspots
        popad
        ret

; WinMain's call to its music check (0x409F00), just after the DirectDraw set-up: put our
; mciSendCommandA back in the import slot (cnc-ddraw's hooks replace it), then run the check.
_brr_hook_music_check:
        pushad
        call    _brr_music_reassert
        popad
        jmp     dword ptr [_brr_orig_music_check]

; The DirectDraw set-up (0x42C590) is entered by a JMP to here (site dd_setup_entry). Its
; arguments: EAX the window, EDX and EBX the display mode's width and height, ECX the frame's
; width, then on the stack the frame's height, the frame buffer and the windowed flag. With the
; new renderer in full screen (render.c), the mode becomes the screen's own size; the frame
; stays the game's. Then the set-up's first instructions (replaced by the JMP), and on.
_brr_hook_dd_setup:
        push    eax
        push    ecx
        push    edx
        movzx   eax, byte ptr [esp + 18h]
        push    eax
        call    _brr_render_mode
        add     esp, 4
        test    eax, eax
        pop     edx
        pop     ecx
        jz      dd_setup_keep
        movzx   edx, ax
        shr     eax, 16
        mov     ebx, eax
dd_setup_keep:
        pop     eax
        push    esi
        push    edi
        push    ebp
        sub     esp, 84h
        jmp     dword ptr [_brr_dd_setup_continue]

; camera_mode4: AX = [ESI+10h] as before, but "previous" ([ESI+46h]) is only
; updated when this frame ran at least one tick (frame_ticks, 44BF80h).
_brr_hook_cam4:
        mov     ax, word ptr [esi + 10h]
        cmp     byte ptr ds:[44BF80h], 0
        je      cam4_done
        mov     word ptr [esi + 46h], ax
cam4_done:
        ret

; Experimental "max out draw distance" (site render_view_entry, 0x4176C4). render_view draws
; one view; its only argument is EAX (no stack arguments; it ends with a plain RET). Every
; reader of the vision distance (detail_level, 44B044h) and texture distance (44B048h) in a
; drawn frame runs inside it, so it runs with ours and the player's are put back afterwards:
; the options, the config file and auto detail never see ours (the game's own trick for its
; special screens, 0x423B5C). EAX, render_view's result, is kept.
_brr_hook_render_view:
        push    dword ptr ds:[44B048h]
        push    dword ptr ds:[44B044h]
        push    eax
        mov     eax, [_brr_draw_vision]
        mov     dword ptr ds:[44B044h], eax
        mov     eax, [_brr_draw_texture]
        mov     dword ptr ds:[44B048h], eax
        pop     eax
        call    render_view_original
        pop     dword ptr ds:[44B044h]
        pop     dword ptr ds:[44B048h]
        ret
render_view_original:
        push    ebx
        push    ecx
        push    edx
        push    esi
        push    edi
        push    ebp
        jmp     dword ptr [_brr_render_view_continue]

        end
