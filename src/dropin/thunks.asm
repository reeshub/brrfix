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
        extrn   _brr_sky_draw:near
        extrn   _brr_render_after_present:near
        extrn   _brr_menu_mouse:near
        extrn   _brr_music_reassert:near
        extrn   _brr_render_mode:near
        extrn   _brr_dd_setup_continue:dword
        extrn   _brr_orig_music_check:dword
        extrn   _brr_render_view_continue:dword
        extrn   _brr_draw_vision:dword
        extrn   _brr_draw_texture:dword
        extrn   _brr_landscape_walk:near
        extrn   _brr_landscape_walk_fn:dword
        extrn   _brr_landscape_walk_loop:dword
        extrn   _brr_landscape_tables_fn:dword
        extrn   _brr_credit_text:near
        extrn   _brr_view_size:near
        extrn   _brr_set_view_continue:dword
        extrn   _brr_screenshot_begin:near
        extrn   _brr_screenshot_end:near
        extrn   _brr_orig_screenshot_race:dword
        extrn   _brr_orig_screenshot_tape:dword
        extrn   _brr_orig_credit_lookup:dword
        extrn   _brr_shade_lo:dword
        extrn   _brr_shade_hi:dword
        extrn   _brr_shade_side_off:dword
        extrn   _brr_shade_delta:dword
        extrn   _brr_shade_vertex_delta:dword
        extrn   _brr_light_level_fn:dword
        extrn   _brr_menu_pad_commit:near
        extrn   _brr_menu_pad_append:near

        public  _brr_TIMER_Init, _brr_TIMER_Shutdown, _brr_TIMER_Handler
        public  _brr_hook_clock_read, _brr_hook_retrace, _brr_hook_race_tick
        public  _brr_hook_race_draw, _brr_hook_camera_update, _brr_hook_rotation
        public  _brr_hook_playback_tick, _brr_hook_playback_draw
        public  _brr_rotation_original, _brr_call_rotation_original
        public  _brr_call_game_atan, _brr_hook_cam4, _brr_hook_present_window
        public  _brr_hook_display_choice, _brr_hook_present, _brr_hook_joystick
        public  _brr_hook_menu_hotspots, _brr_hook_menu_mouse, _brr_hook_music_check
        public  _brr_hook_dd_setup, _brr_hook_render_view, _brr_hook_landscape_walk
        public  _brr_hook_sky_draw, _brr_call_sky_draw
        public  _brr_call_landscape_walk, _brr_call_landscape_tables, _brr_call_landscape_far_walk
        public  _brr_hook_credit_lookup, _brr_hook_set_view, _brr_call_set_view
        public  _brr_hook_screenshot_race, _brr_hook_screenshot_tape
        public  _brr_span_gouraud, _brr_span_gouraud_z, _brr_poly_light_smooth
        public  _brr_call_palette_set
        public  _brr_hook_scn_commit, _brr_hook_scn_end
        public  _brr_call_dd_set_mode

        .code

; The game's TIMER.DLL exports, which PATCH.DLL provides (named in dropin_build.py's linker directives).
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
        mov     [esp + 28], eax                 ; EAX after POPAD: 1 = presented in full colour
        popad
        test    eax, eax
        jz      present_not_done
        mov     eax, 1
        jmp     present_done
present_not_done:
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

; Site sky_draw_call (render_view, 0x4176F6): CALL sky_draw(ground, sky), stdcall, with the
; game's code keeping every register but EAX. brr_sky_draw (cdecl) records the view and calls it.
_brr_hook_sky_draw:
        push    ecx
        push    edx
        push    dword ptr [esp + 16]            ; sky
        push    dword ptr [esp + 16]            ; ground
        call    _brr_sky_draw
        add     esp, 8
        pop     edx
        pop     ecx
        ret     8

; Site scn_commit_call: scn_load's CALL menu_hotspot_commit (0x427FAC, which keeps EBX, ECX
; and EDX): the game's commit, then brr_menu_pad_commit(the count before it)
_brr_hook_scn_commit:
        push    ecx
        push    edx
        movsx   eax, word ptr ds:[44BF38h]
        push    eax
        mov     eax, 427FACh
        call    eax
        call    _brr_menu_pad_commit
        add     esp, 4
        pop     edx
        pop     ecx
        ret

; Site scn_end_call: scn_load's last call, menu_background_save (0x412EAC): our twins are
; added first, then the game's routine runs as before (a tail jump)
_brr_hook_scn_end:
        push    ecx
        push    edx
        call    _brr_menu_pad_append
        pop     edx
        pop     ecx
        mov     eax, 412EACh
        jmp     eax

; brr_call_dd_set_mode(width, height), cdecl: the game's dd_set_mode (0x42D044; EAX the
; width, EDX the height, BL 0: no swap between window and full screen; it keeps ECX, ESI,
; EDI and EBP): DirectDraw re-created at that size, through dd_setup (site dd_setup_entry)
_brr_call_dd_set_mode:
        push    ebx
        mov     eax, [esp + 8]
        mov     edx, [esp + 12]
        xor     ebx, ebx
        mov     ecx, 42D044h
        call    ecx
        pop     ebx
        ret

; brr_call_palette_set(rgb, first, count), cdecl: the game's palette_set (0x42D1E8; EAX the
; 6-bit entries, EDX the first, EBX the count; it keeps every register but EAX)
_brr_call_palette_set:
        push    ebx
        mov     eax, [esp + 8]
        mov     edx, [esp + 12]
        mov     ebx, [esp + 16]
        mov     ecx, 42D1E8h
        call    ecx
        pop     ebx
        ret

; brr_call_sky_draw(ground, sky), cdecl: the game's sky_draw (0x41A4D0, stdcall; it keeps
; EBX, ESI, EDI and EBP)
_brr_call_sky_draw:
        push    dword ptr [esp + 8]             ; sky
        push    dword ptr [esp + 8]             ; ground
        mov     eax, 41A4D0h
        call    eax
        ret

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

; set_view (0x425F78; width in EAX, height in EDX) is entered by a JMP to here (site
; set_view_entry): brr_view_size may give a bigger race view (render.c, Max). Then set_view's
; first instructions (replaced by the JMP), and on. set_view itself changes EAX and EDX.
_brr_hook_set_view:
        push    ecx
        push    edx
        push    eax
        call    _brr_view_size
        add     esp, 8
        pop     ecx
        mov     edx, eax
        shr     edx, 16
        and     eax, 0FFFFh
        push    ebx
        push    ecx
        push    esi
        mov     esi, edx
        jmp     dword ptr [_brr_set_view_continue]

; cdecl brr_call_set_view(width, height): the game's set_view (0x425F78, through site
; set_view_entry). It keeps EBX, ECX and ESI; EDI and EBP it doesn't touch.
_brr_call_set_view:
        mov     eax, [esp + 4]
        mov     edx, [esp + 8]
        push    ebx
        push    esi
        push    edi
        mov     ebx, 425F78h
        call    ebx
        pop     edi
        pop     esi
        pop     ebx
        ret

; The race's screenshots (run_screen_frame's CALLs at 0x428D9E and 0x428D62): with Max,
; render.c puts the view back to 640x480 around them. Every register is kept.
_brr_hook_screenshot_race:
        push    dword ptr [_brr_orig_screenshot_race]
        jmp     screenshot_common
_brr_hook_screenshot_tape:
        push    dword ptr [_brr_orig_screenshot_tape]
screenshot_common:
        pushad
        call    _brr_screenshot_begin
        popad
        call    dword ptr [esp]
        pushad
        call    _brr_screenshot_end
        popad
        add     esp, 4
        ret

; The credits ticker's text lookup (0x423511, site credit_lookup_call): EAX = the entry's
; name. brr_credit_text gives our line, or 0 for the game's own lookup (0x42818C) of the
; name, which it may have renumbered. Returns the text in EAX; every other register kept.
_brr_hook_credit_lookup:
        push    ecx
        push    edx
        push    eax
        call    _brr_credit_text
        pop     edx
        test    eax, eax
        jnz     credit_done
        mov     eax, edx
        call    dword ptr [_brr_orig_credit_lookup]
credit_done:
        pop     edx
        pop     ecx
        ret

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

; Site landscape_walk_call: render_view's CALL 0x41D5E4 (the landscape walk) comes here:
; distance.c runs the walk, then the far passes. EAX is the walk's result.
_brr_hook_landscape_walk:
        pushad
        call    _brr_landscape_walk
        mov     [esp + 28], eax
        popad
        ret

; For distance.c: the game's landscape walk (0x41D5E4) and camera-space tables (0x41D24C),
; with every register kept.
_brr_call_landscape_walk:
        pushad
        call    dword ptr [_brr_landscape_walk_fn]
        mov     [esp + 28], eax
        popad
        ret
_brr_call_landscape_tables:
        pushad
        call    dword ptr [_brr_landscape_tables_fn]
        popad
        ret

; One far pass: the walk on the grid as distance.c filled it. Its first instructions as
; at 0x41D5E4 (the six PUSHes its end POPs, then the grid and the first column), but not
; its CALL 0x42B854, which would start the landscape's polygon records again; then its
; loop at 0x41D618, which returns here.
_brr_call_landscape_far_walk:
        pushad
        call    far_walk_entry
        popad
        ret
far_walk_entry:
        push    ebx
        push    ecx
        push    edx
        push    esi
        push    edi
        push    ebp
        mov     eax, dword ptr ds:[44BA14h]
        mov     dword ptr ds:[46BBACh], eax
        mov     ax, word ptr ds:[46BBC0h]
        mov     word ptr ds:[46BBDAh], ax
        mov     ax, word ptr ds:[46BBDCh]
        mov     word ptr ds:[46BBD0h], ax
        jmp     dword ptr [_brr_landscape_walk_loop]

; ---- Smooth shading (shade.c): copies of the 3D engine's Gouraud spans and smooth lighting ----
; Each does exactly what the original does, instruction for instruction, plus one store. The
; spans keep each drawn pixel's palette index and the bits 8-15 of its 16.16 shade (which the
; original drops) as a word in shade.c's side buffer, at brr_shade_side_off + 2 x the pixel's
; address; a polygon outside the memory the side buffer covers goes to the original.

; span_gouraud_z (0x4438D0): ESI the left edge, EBP the right edge (0x34-byte records), EDI
; the row, EAX the rows left (16.16); depth at [0x54203C] + 2 x the pixel's address.
_brr_span_gouraud_z:
        cmp     edi, _brr_shade_lo
        jb      sgz_original
        mov     ebx, eax
        sar     ebx, 16
        inc     ebx
        imul    ebx, dword ptr ds:[542018h]
        add     ebx, edi
        cmp     ebx, _brr_shade_hi
        ja      sgz_original
        mov     ebx, _brr_shade_side_off
        sub     ebx, dword ptr ds:[54203Ch]
        mov     _brr_shade_delta, ebx           ; side word = depth pointer + delta
sgz_row:
        xor     ebx, ebx
        xor     ecx, ecx
        mov     bx, word ptr [esi + 1Ah]
        push    edi
        add     edi, ebx
        mov     cx, word ptr [ebp + 1Ah]
        sub     cx, bx
        jle     sgz_next
        push    eax
        mov     ebx, dword ptr [ecx*4 + 54A384h]
        push    esi
        push    ebp
        mov     dword ptr ds:[54A15Ch], ecx
        mov     ecx, dword ptr [esi + 1Ch]
        mov     eax, dword ptr [ebp + 1Ch]
        push    ecx
        sub     eax, ecx
        imul    ebx
        mov     eax, dword ptr [ebp + 20h]
        mov     ebp, edx
        mov     ecx, dword ptr [esi + 20h]
        mov     esi, dword ptr ds:[54203Ch]
        sub     eax, ecx
        imul    ebx
        lea     esi, [esi + edi*2]
        pop     eax
sgz_pixel:
        shld    ebx, eax, 16
        add     eax, ebp
        cmp     bx, word ptr [esi]
        jc      sgz_hidden
        mov     word ptr [esi], bx
        shld    ebx, ecx, 16
        mov     byte ptr [edi], bl
        mov     ebx, ecx                        ; ours: BH the index, BL the fraction
        shr     ebx, 8
        add     esi, _brr_shade_delta
        mov     word ptr [esi], bx
        sub     esi, _brr_shade_delta
sgz_hidden:
        add     ecx, edx
        add     esi, 2
        inc     edi
        dec     dword ptr ds:[54A15Ch]
        jnz     sgz_pixel
        pop     ebp
        pop     esi
        pop     eax
sgz_next:
        mov     ebx, dword ptr [ebp]
        mov     ecx, dword ptr [ebp + 4]
        mov     edx, dword ptr [ebp + 8]
        add     dword ptr [ebp + 18h], ebx
        add     dword ptr [ebp + 1Ch], ecx
        add     dword ptr [ebp + 20h], edx
        pop     edi
        mov     ebx, dword ptr [esi]
        mov     ecx, dword ptr [esi + 4]
        mov     edx, dword ptr [esi + 8]
        add     dword ptr [esi + 18h], ebx
        add     dword ptr [esi + 1Ch], ecx
        add     dword ptr [esi + 20h], edx
        add     edi, dword ptr ds:[542018h]
        sub     eax, 10000h
        js      sgz_done
        cmp     eax, dword ptr [esi + 30h]
        js      sgz_left
        cmp     eax, dword ptr [ebp + 30h]
        jns     sgz_row
        add     ebp, 34h
        jmp     sgz_row
sgz_left:
        add     esi, 34h
        cmp     eax, dword ptr [ebp + 30h]
        jns     sgz_row
        add     ebp, 34h
        jmp     sgz_row
sgz_done:
        inc     word ptr ds:[542B58h]
        ret
sgz_original:
        push    4438D0h
        ret

; span_gouraud (0x443710): the same without depth; EAX is free in the pixel loop
_brr_span_gouraud:
        cmp     edi, _brr_shade_lo
        jb      sg_original
        mov     ebx, eax
        sar     ebx, 16
        inc     ebx
        imul    ebx, dword ptr ds:[542018h]
        add     ebx, edi
        cmp     ebx, _brr_shade_hi
        ja      sg_original
sg_row:
        mov     ebx, dword ptr [esi + 18h]
        mov     ecx, dword ptr [ebp + 18h]
        sar     ebx, 16
        sar     ecx, 16
        push    edi
        add     edi, ebx
        sub     ecx, ebx
        jle     sg_next
        mov     ebx, dword ptr [ecx*4 + 54A384h]
        push    eax
        push    ebp
        mov     eax, dword ptr [ebp + 20h]
        mov     ebp, dword ptr [esi + 20h]
        sub     eax, ebp
        imul    ebx
        shld    ebx, ebp, 16
        lea     eax, [edi + edi]                ; ours: the side word's address
        add     eax, _brr_shade_side_off
sg_pixel:
        mov     byte ptr [edi], bl
        mov     ebx, ebp                        ; ours: BH the index, BL the fraction
        shr     ebx, 8
        mov     word ptr [eax], bx
        add     eax, 2
        add     ebp, edx
        mov     ebx, ebp
        inc     edi
        shr     ebx, 16
        dec     ecx
        jnz     sg_pixel
        pop     ebp
        pop     eax
sg_next:
        mov     ebx, dword ptr [ebp]
        mov     edx, dword ptr [ebp + 8]
        add     dword ptr [ebp + 18h], ebx
        add     dword ptr [ebp + 20h], edx
        pop     edi
        mov     ebx, dword ptr [esi]
        mov     edx, dword ptr [esi + 8]
        add     dword ptr [esi + 18h], ebx
        add     dword ptr [esi + 20h], edx
        add     edi, dword ptr ds:[542018h]
        sub     eax, 10000h
        js      sg_done
        cmp     eax, dword ptr [esi + 30h]
        js      sg_left
        cmp     eax, dword ptr [ebp + 30h]
        jns     sg_row
        add     ebp, 34h
        jmp     sg_row
sg_left:
        add     esi, 34h
        cmp     eax, dword ptr [ebp + 30h]
        jns     sg_row
        add     ebp, 34h
        jmp     sg_row
sg_done:
        inc     word ptr ds:[542B58h]
        ret
sg_original:
        push    443710h
        ret

; poly_light_smooth (0x446D70): for each corner of the polygon ring (0x542B70), light_level
; (0x4468E0, EAX = 1) and the shade table's index for it and the colour (0x542B74); ours
; writes the whole 16.16 shade at +18h, with shade.c's fraction, where the original wrote
; only the index at +1Ah
_brr_poly_light_smooth:
        mov     eax, dword ptr ds:[54A1E4h]
        mov     edi, dword ptr ds:[542B70h]
        mov     dword ptr ds:[54A1E8h], eax
pls_corner:
        mov     ecx, dword ptr [edi + 8]
        mov     eax, dword ptr [edi]
        sar     ecx, 16
        mov     ebx, dword ptr [edi + 4]
        mov     dword ptr ds:[54B4A4h], eax
        mov     dword ptr ds:[54B4A8h], ebx
        mov     ebx, dword ptr [edi + 30h]
        mov     dword ptr ds:[54B4ACh], ecx
        mov     dword ptr ds:[54B4A0h], ebx
        mov     eax, 1
        call    dword ptr [_brr_light_level_fn]
        mov     ebx, dword ptr ds:[542B74h]
        shl     eax, 8
        and     ebx, 0FFh
        mov     ecx, dword ptr ds:[54A35Ch]
        add     ebx, eax
        movzx   eax, byte ptr [ecx + ebx]
        push    edx                             ; ours: index x 65536 + the fraction
        push    eax
        mov     edx, _brr_shade_vertex_delta
        movsx   edx, word ptr [edx + ebx*2]
        shl     eax, 16
        add     eax, edx
        mov     dword ptr [edi + 18h], eax
        pop     eax
        pop     edx
        mov     edi, dword ptr [edi + 28h]
        cmp     edi, dword ptr ds:[542B70h]
        jnz     pls_corner
        ret

        end
