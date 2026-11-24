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
        extrn   _brr_reciprocals:dword
        extrn   _brr_proj_scale:dword
        extrn   _brr_count_project:dword
        extrn   _brr_count_backface:dword
        extrn   _brr_count_clip:dword
        extrn   _brr_count_unlit:dword
        extrn   _brr_shading_threshold:near
        extrn   _brr_lod_select:near
        extrn   _brr_lod_scale:near
        extrn   _brr_landscape_width:near
        extrn   _brr_hud_begin:near
        extrn   _brr_hud_end:near
        extrn   _brr_hud_sprite:near
        extrn   _brr_hud_drawing:dword
        extrn   _brr_orig_hud_draw:dword
        extrn   _brr_orig_hud_icon:dword
        extrn   _brr_raw_loaded:near
        extrn   _brr_splash_shown:near
        extrn   _brr_splash_hold:near
        extrn   _brr_persp_setup:near
        extrn   _brr_persp_setup_original:dword
        extrn   _brr_horizon_cull:near
        extrn   _brr_horizon_drawn:near
        extrn   _brr_sound_debug:near
        extrn   _brr_sound_debug_on:dword
        extrn   _brr_ds_pans:dword
        extrn   _brr_sprite_scaled_continue:dword
        extrn   _brr_orig_race_tick:dword
        extrn   _brr_orig_race_draw:dword
        extrn   _brr_orig_playback_tick:dword
        extrn   _brr_orig_playback_draw:dword
        extrn   _brr_orig_preview_tick:dword
        extrn   _brr_orig_preview_draw:dword
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
        extrn   _brr_orig_hotspot_draw:dword
        extrn   _brr_orig_text_draw:dword
        extrn   _nav_label_font:near
        extrn   _brr_orig_sprite_draw:dword
        extrn   _nav_sprite_frame:near
        public  _brr_sound_sprite_down, _brr_sound_sprite_up, _brr_sound_sprite_up2, _brr_knob_sprite
        extrn   _brr_sound_cd_draw:near
        public  _brr_hook_sound_draw, _brr_call_sprite_draw, _brr_call_font_char
        extrn   _nav_before_draw:near
        extrn   _nav_after_draw:near
        public  _brr_hook_hotspot_draw, _brr_call_play_sample, _brr_hook_label_draw
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
        public  _brr_hook_preview_tick, _brr_hook_preview_draw
        public  _brr_rotation_original, _brr_call_rotation_original
        public  _brr_call_game_atan, _brr_hook_cam4, _brr_hook_present_window
        public  _brr_hook_display_choice, _brr_hook_present, _brr_hook_joystick
        public  _brr_hook_menu_hotspots, _brr_hook_menu_mouse, _brr_hook_music_check
        public  _brr_hook_dd_setup, _brr_hook_render_view, _brr_hook_landscape_walk
        public  _brr_hook_sky_draw, _brr_call_sky_draw
        public  _brr_call_landscape_walk, _brr_call_landscape_tables, _brr_call_landscape_far_walk
        public  _brr_hook_credit_lookup, _brr_hook_set_view
        public  _brr_span_gouraud, _brr_span_gouraud_z, _brr_poly_light_smooth
        public  _brr_call_palette_set
        public  _brr_hook_scn_commit, _brr_hook_scn_end
        public  _brr_call_dd_set_mode
        public  _brr_hook_sprite_rows, _brr_hook_sprite_columns
        public  _brr_hook_hud_draw, _brr_hook_hud_icon, _brr_hook_sprite_scaled
        public  _brr_hook_raw_name, _brr_hook_persp_setup, _brr_hook_scenery_pos, _brr_hook_display_list
        public  _brr_hook_splash_present, _brr_hook_frame_message
        public  _brr_hook_sound_nearest, _brr_hook_ds_pan
        public  _brr_hook_proj_scale, _brr_hook_landscape_width
        public  _brr_backface, _brr_clip_t_x, _brr_clip_t_y, _brr_cull_margin, _brr_shading_restore
        public  _brr_project_x, _brr_project_y, _brr_hook_shading_threshold, _brr_hook_lod
        public  _brr_hook_fit_depth

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

; Track select's View (screen 0x10, track_preview: the helicopter flyover): its tick and draw,
; wrapped like the race's, for interpolation and the mouse's first second
_brr_hook_preview_tick:
        pushad
        call    _brr_before_tick
        popad
        call    dword ptr [_brr_orig_preview_tick]
        pushad
        call    _brr_after_tick
        popad
        ret

_brr_hook_preview_draw:
        pushad
        call    _brr_before_draw
        popad
        call    dword ptr [_brr_orig_preview_draw]
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

; Site hotspot_draw_call: menu_draw_group's CALL menu_hotspot_draw (0x427C7C, the hotspot's
; index in AX, which keeps EBX, ECX and EDX): the joystick menu navigation's notes either
; side of it (menunav.c: what was drawn, the selected item's font or frame, where its text
; ends). Every register is kept.
_brr_hook_hotspot_draw:
        push    eax
        pushad
        movsx   eax, ax
        push    eax
        call    _nav_before_draw
        add     esp, 4
        popad
        call    dword ptr [_brr_orig_hotspot_draw]
        pushad
        movsx   eax, word ptr [esp + 32]
        push    eax
        call    _nav_after_draw
        add     esp, 4
        popad
        add     esp, 4
        ret

; Site label_draw_call: joystick_options_draw_type's CALL text_draw (0x4124A0; EAX the text,
; EDX 0, EBX 0x7FFF, text_x and text_y already set): nav_label_font gives the label its line's
; font (menunav.c), then text_draw, returning to the game's caller. Every register is kept.
_brr_hook_label_draw:
        pushad
        call    _nav_label_font
        popad
        jmp     dword ptr [_brr_orig_text_draw]

; sound_options_draw's CALL sprite_draw (0x426D48) of a button (EDX its frame, EBX x, ECX y;
; sites sound_button_*): the frame nav_sprite_frame gives (its step of the screen's animation
; kept in the joystick's selection, menunav.c), then sprite_draw. Each entry is how the
; button's frame follows the step; _brr_knob_sprite (sites slider_knob_*: the display and
; damage screens' knobs, drawn from animation 1's frame) passes 0.
_brr_knob_sprite:
        push    0
        jmp     short sound_sprite
_brr_sound_sprite_down:
        push    -1
        jmp     short sound_sprite
_brr_sound_sprite_up:
        push    1
        jmp     short sound_sprite
_brr_sound_sprite_up2:
        push    2
sound_sprite:
        pushad
        push    dword ptr [esp + 32]
        push    ecx
        push    ebx
        movsx   edx, dx
        push    edx
        call    _nav_sprite_frame
        add     esp, 16
        mov     [esp + 20], eax
        popad
        add     esp, 4
        jmp     dword ptr [_brr_orig_sprite_draw]

; sound_options_draw's CALL menu_draw_group with EAX 1 (0x40B020; site sound_cd_draw): the CD
; volume's -, + and level first (music.c), then groups 1 and 21 (the CD's name), as DOS's
; sound_options_draw does. menu_draw_group (0x427DD8) takes the group in EAX; what follows the
; site (present, then the pops) needs no register kept.
_brr_hook_sound_draw:
        pushad
        call    _brr_sound_cd_draw
        popad
        mov     eax, 1
        mov     edx, 427DD8h
        call    edx
        mov     eax, 15h
        mov     edx, 427DD8h
        jmp     edx

; brr_call_sprite_draw(frame, x, y), cdecl: the game's sprite_draw (0x426D48) from the
; screen's sprite bank (menu_sprites 0x44BF34): EAX the bank, EDX the frame, EBX x, ECX y
_brr_call_sprite_draw:
        push    ebx
        push    esi
        push    edi
        push    ebp
        mov     eax, dword ptr ds:[44BF34h]
        mov     edx, [esp + 20]
        mov     ebx, [esp + 24]
        mov     ecx, [esp + 28]
        call    dword ptr [_brr_orig_sprite_draw]
        pop     ebp
        pop     edi
        pop     esi
        pop     ebx
        ret

; brr_call_font_char(ch, font, x, y), cdecl: the game's font_draw_sprite_char (0x412918; EAX
; the character, EDX the font slot, EBX x, ECX y, into the view at 0x542030; it changes EBX,
; ECX and EDX) for hud.c's prompt; returns the glyph's advance (AX, sign-extended)
_brr_call_font_char:
        push    ebx
        push    esi
        push    edi
        push    ebp
        mov     eax, [esp + 20]
        mov     edx, [esp + 24]
        mov     ebx, [esp + 28]
        mov     ecx, [esp + 32]
        mov     esi, 412918h
        call    esi
        movsx   eax, ax
        pop     ebp
        pop     edi
        pop     esi
        pop     ebx
        ret

; brr_call_play_sample(name), cdecl: the game's sound_play_sample (0x415A28; EAX the
; sample's name, played from SAMPLES\<name>.WAV once; it keeps EBX, ECX and EDX)
_brr_call_play_sample:
        push    ebx
        mov     eax, [esp + 8]
        mov     ebx, 415A28h
        call    ebx
        pop     ebx
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

; ---- The 3D engine's maths at any resolution ([display] precise_3d; KNOWLEDGE "Exact 3D maths") ----
; The engine was written for 320x200 and 640x480, where its 32-bit products never overflowed.
; Projection scales grow with the picture's height (1,536 at 1,440 rows), so they do now.

; Sites project_x and project_y (model_op_vertices 0x4413B0, 0x4413C5: IMUL EAX,[scale]; CDQ,
; before IDIV by the depth): the product in EDX:EAX. x and y are view space x 256, so the game's
; 32-bit product overflowed for corners more than about 10.7 squares to the side at 1,440 rows
; (32 at 640x480). The quotient stays within 3.4 million (|x| < 2^23, scale < 6,600, depth >=
; 16,384): the IDIV can't overflow. Each product that wouldn't fit 32 bits is counted (OF).
_brr_project_x:
        imul    dword ptr ds:[542000h]
        jno     px_ok
        inc     dword ptr [_brr_count_project]
px_ok:
        ret

_brr_project_y:
        imul    dword ptr ds:[542004h]
        jno     py_ok
        inc     dword ptr [_brr_count_project]
py_ok:
        ret

; Sites backface_model (poly_cull_outcodes 0x445C19) and backface_polygon (poly_cull_visible
; 0x445E39): the screen cross product (x1 - x0)(y2 - y0) - (x2 - x0)(y1 - y0), whose sign drops
; back faces, in 64 bits. On entry, as at the site: EAX = x2 - x0, EDX = y1 - y0, EDI = x1,
; EBX = x0, ECX = y0, ESI = the third vertex (y at +0Ch). Leaves SF the result's sign for the
; game's JS; EAX, EBX, EDX and EDI are reloaded after it.
_brr_backface:
        sub     edi, ebx
        mov     ebx, dword ptr [esi + 0Ch]
        sub     ebx, ecx
        imul    edx
        push    edx
        push    eax
        mov     eax, edi
        imul    ebx
        sub     eax, dword ptr [esp]
        sbb     edx, dword ptr [esp + 4]
        lea     esp, [esp + 8]
        mov     edi, edx                        ; the game's 32-bit result is EAX: counted when
        xor     edi, eax                        ; its sign differs from the true one's
        jns     bf_same
        inc     dword ptr [_brr_count_backface]
bf_same:
        test    edx, edx                        ; SF: the true sign, for the game's JS
        mov     edi, edx
        ret

; Sites clip_x_t (clip_edge_x 0x4431F9) and clip_y_t (clip_edge_y 0x443289): the fraction of the
; edge at the screen's edge, (v0 - edge) x 65536 / (v0 - v1), with the dividend in 64 bits (the
; game shifts it left 16 in 32: wrong beyond 32,767 pixels off screen). On entry EAX = v0 - edge,
; ECX = v1 - v0, EBX = the edge, ESI = the new vertex; leaves the fraction in ECX and the vertex
; on the edge, or ECX = 0 for a level edge, as the game.
_brr_clip_t_x:
        neg     ecx
        jz      ctx_done
        lea     edx, [eax + 8000h]              ; counted when the game's << 16 would overflow
        cmp     edx, 0FFFFh
        jbe     ctx_fits
        inc     dword ptr [_brr_count_clip]
ctx_fits:
        cdq
        shld    edx, eax, 16
        shl     eax, 16
        idiv    ecx
        mov     dword ptr [esi + 10h], ebx
        mov     ecx, eax
ctx_done:
        ret

_brr_clip_t_y:
        neg     ecx
        jz      cty_done
        lea     edx, [eax + 8000h]
        cmp     edx, 0FFFFh
        jbe     cty_fits
        inc     dword ptr [_brr_count_clip]
cty_fits:
        cdq
        shld    edx, eax, 16
        shl     eax, 16
        idiv    ecx
        mov     dword ptr [esi + 0Ch], ebx
        mov     ecx, eax
cty_done:
        ret

; Site cull_margin (object_cull_sphere 0x4487B8: SHL [0x54B498],7, the radius x 128): the radius
; x the larger projection scale, so an object is culled at the screen's edges only once all of
; it is off screen (x 128 was half the radius at 320 wide, a twelfth at 1,440 rows). EAX and EDX
; are reloaded after it.
_brr_cull_margin:
        mov     eax, dword ptr ds:[542004h]
        cmp     eax, dword ptr ds:[542000h]
        jge     cm_mul
        mov     eax, dword ptr ds:[542000h]
cm_mul:
        imul    eax, dword ptr ds:[54B498h]
        mov     dword ptr ds:[54B498h], eax
        ret

; Sites unlit_restore_6 and unlit_restore_9 (display_list_draw 0x42BAFF, 0x42BB26): after an
; unlit node (effects: dust, smoke) the game put back the near objects' smooth lighting (mode 2)
; but, before the list reached them (DI = 0), left the engine unlit, so every far object drawn
; after it came out unlit until a mode-1 node. Here: mode 2 if DI = 2, else 1 (flat, as the far
; objects are drawn). engine_set_shading (0x446860) takes its mode on the stack and changes EAX.
_brr_shading_restore:
        mov     eax, 1
        cmp     di, 2
        je      sr_near
        inc     dword ptr [_brr_count_unlit]    ; where the game left the far objects unlit
        jmp     sr_set
sr_near:
        mov     eax, 2
sr_set:
        push    eax
        mov     eax, 446860h
        call    eax
        ret

; Site lod_select (object_draw 0x4411BE: CMP dword ptr [0x54B488],0 before the level-of-detail
; loop; ESI = the model's table of (depth, offset) pairs, ended by -1; the depth at 0x54B488 is
; 16.16) -> CALL here: brr_lod_select (render.c) sets brr_lod_depth, which the loop's compare
; reads (site lod_compare); then the game's CMP, for its JS after.
_brr_hook_lod:
        pushad
        push    esi
        call    _brr_lod_select
        add     esp, 4
        popad
        cmp     dword ptr ds:[54B488h], 0
        ret

; Site fit_depth (scenery_fit_lods_to_ground 0x42B744, where its two branches meet: XOR EAX,EAX;
; MOV AX,[ECX+22h]; ESI = the object's centre depth, ECX its model) -> CALL here and a NOP:
; ESI the depth as the level-of-detail choice sees it (brr_lod_scale), so the version that's
; drawn is the one fitted to the ground; then the two instructions. EBX and EDX are reloaded
; after.
_brr_hook_fit_depth:
        push    ecx
        push    esi
        call    _brr_lod_scale
        add     esp, 4
        mov     esi, eax
        pop     ecx
        xor     eax, eax
        mov     ax, word ptr [ecx + 22h]
        ret

; Site shading_threshold (display_list_draw 0x42B8BA, 31 bytes: EBP = clamp(vision - 9, 3, 8),
; the distance in squares inside which objects get smooth vertex lighting, and XOR EDI,EDI)
; -> CALL here, then XOR EDI,EDI: brr_shading_threshold ([display] object_shading) chooses it.
_brr_hook_shading_threshold:
        push    ecx
        push    edx
        call    _brr_shading_threshold
        mov     ebp, eax
        pop     edx
        pop     ecx
        ret

; Site proj_scale_store (set_view 0x425FA1: MOV [0x542000],EAX, the horizontal projection
; scale, width x 256 / 320) -> CALL here: a widescreen view's from its height instead
; (brr_proj_scale, render.c; 0 = the game's), so the view shows more at the sides rather than
; less from top to bottom. The vertical scale follows from EAX as before; EDX (the
; high-resolution flag) and the rest kept.
_brr_hook_proj_scale:
        cmp     dword ptr [_brr_proj_scale], 0
        je      ps_store
        mov     eax, dword ptr [_brr_proj_scale]
ps_store:
        mov     dword ptr ds:[542000h], eax
        ret

; Site landscape_width (landscape_setup 0x41CD16, EAX = 7 x vision: MOV EDX,EAX; MOV EBX,10;
; SAR EDX,31; IDIV EBX, the visible ground's half-width - 1) -> CALL here: the same division,
; then brr_landscape_width (render.c) widens it for a widescreen view. EBX 10 and the
; remainder in EDX, as the game leaves them.
_brr_hook_landscape_width:
        mov     edx, eax
        mov     ebx, 0Ah
        sar     edx, 1Fh
        idiv    ebx
        push    ecx
        push    edx
        push    eax
        call    _brr_landscape_width
        add     esp, 4
        pop     edx
        pop     ecx
        ret

; Sites hud_draw_call (render_view 0x417865) and hud_icon_call (0x417877): CALL hud_draw
; (0x423658, the vehicle in EAX) and CALL hud_draw_playback_icon (0x4236F0) -> here: the game's
; 2D drawing pointed at our 320x200 layers around them (hud.c). Every register as the game's
; own call leaves it, but EAX, which render_view doesn't read after either.
_brr_hook_hud_draw:
        pushad
        push    1
        call    _brr_hud_begin
        add     esp, 4
        popad
        call    dword ptr [_brr_orig_hud_draw]
        pushad
        call    _brr_hud_end
        popad
        ret

; Site persp_setup_hook (engine_draw_polygon type 4 stores this in poly_setup_hook 0x5430D4
; instead of poly_setup_perspective 0x446FF0; called once per polygon by 0x442B04 and 0x442C42,
; which rely on EBX, ECX, ESI and EBP afterwards): without the depth buffer (render_flags
; 0x542008 bit 0x10, how the ground is drawn) brr_persp_setup (persp.c) sets the polygon up
; with more precision; with it, or if that declines (returns 0), the game's own set-up.
_brr_hook_persp_setup:
        test    dword ptr ds:[542008h], 10h
        jz      short ps_game                   ; depth buffer on: the game's set-up
        pushfd
        pushad
        cld
        call    _brr_persp_setup                ; EAX = 1 done, 0 not
        test    eax, eax
        popad                                   ; keeps the flags from TEST
        jz      short ps_fallback
        popfd
        ret
ps_fallback:
        popfd
ps_game:
        jmp     dword ptr [_brr_persp_setup_original]   ; 0x446FF0, which returns to our caller

; Site sound_nearest_first (sound_update_vehicles 0x416695-0x4166D3, its loop choosing the next
; vehicle whose sounds to gather: the lowest-numbered one not yet done, active (+0x26 >= 0) and
; nearer than 0x7FFF) -> CALL here + NOPs: the nearest such vehicle instead (ties: the lower
; number), in ESI, or -1. The caller's frame: distances at [ESP+0x1C] (dwords), done flags at
; [ESP+0x48] (bytes); here 4 more for the return address. EAX, EBX and EDX are set afresh after
; it; EDI (0, the priority) and EBP are kept.
_brr_hook_sound_nearest:
        cmp     dword ptr [_brr_sound_debug_on], 0
        je      short sn_start
        pushad
        lea     eax, [esp + 32 + 4]             ; the caller's frame: pans, then distances at +0x1C
        push    eax
        call    _brr_sound_debug
        add     esp, 4
        popad
sn_start:
        mov     esi, -1
        mov     ebx, 7FFFh
        xor     eax, eax
sn_loop:
        cmp     byte ptr [esp + eax + 4 + 48h], 0
        jne     short sn_next
        imul    edx, eax, 395h
        cmp     word ptr [edx + 452A90h + 26h], 0
        jl      short sn_next
        mov     edx, [esp + eax*4 + 4 + 1Ch]
        cmp     edx, ebx
        jge     short sn_next
        mov     ebx, edx
        mov     esi, eax
sn_next:
        inc     eax
        cmp     eax, 7
        jl      short sn_loop
        ret

; Site ds_pan_curve (ds_set_pan 0x42ECC3-0x42ECF0: pan x 10,000 / 127, clamped) -> CALL here +
; NOPs: EDX, the pan (-127 to 127, sign-extended), becomes DirectSound's from brr_ds_pans (sound.c).
; EBX (the channel) is kept; EAX and ECX are set afresh after it.
_brr_hook_ds_pan:
        cmp     edx, -127
        jge     short dp_low
        mov     edx, -127
dp_low:
        cmp     edx, 127
        jle     short dp_high
        mov     edx, 127
dp_high:
        mov     edx, dword ptr [_brr_ds_pans + 127 * 4 + edx * 4]
        ret

; Site display_list_call (render_view 0x417808: CALL display_list_draw) -> here: horizon culling
; (horizon.c) takes out what nearer ground will paint over, then display_list_draw (0x42B8B4),
; which returns to render_view.
_brr_hook_display_list:
        pushad
        call    _brr_horizon_cull
        popad
        mov     eax, 42B8B4h
        call    eax
        pushad
        call    _brr_horizon_drawn
        popad
        ret

; Site scenery_snap (object_transform 0x448D06: MOV EAX,[ESI]; MOV EBX,[ESI+4]; MOV ECX,[ESI+8],
; the object's position, for drawing) -> CALL here + 3 NOPs: the same loads; then, for the level's
; own objects (scenery_objects, 39-byte records from *0x498054, the count a word at 0x498572),
; an x or z one unit past a multiple of 256 is put back on it. The level editor placed about half
; the pieces meant to meet (tunnel sections, the start's billboards) one unit apart, a slit of
; sky at today's resolutions. Drawing only: the records themselves aren't changed. EDX and the
; flags are kept (both paths into 0x448D06 have EDX set).
_brr_hook_scenery_pos:
        mov     eax, [esi]
        mov     ebx, [esi + 4]
        mov     ecx, [esi + 8]
        pushfd
        push    edx
        push    ebx
        mov     edx, dword ptr ds:[498054h]
        cmp     esi, edx
        jb      short sp_done
        movzx   ebx, word ptr ds:[498572h]
        imul    ebx, ebx, 39
        add     ebx, edx
        cmp     esi, ebx
        jae     short sp_done
        mov     edx, eax
        and     edx, 0FFh
        cmp     edx, 1
        jne     short sp_z
        dec     eax
sp_z:
        mov     edx, ecx
        and     edx, 0FFh
        cmp     edx, 1
        jne     short sp_done
        dec     ecx
sp_done:
        pop     ebx
        pop     edx
        popfd
        ret

; Site raw_name_call (raw_picture_load 0x412B2F: CALL file_open, the .RAW's name in EAX) -> here:
; brr_raw_loaded notes it (render.c, the widescreen menu backgrounds), then file_open (0x411B78),
; which returns to raw_picture_load.
_brr_hook_raw_name:
        pushad
        push    eax
        call    _brr_raw_loaded
        add     esp, 4
        popad
        mov     edx, 411B78h
        jmp     edx

; Site splash_present (game_init 0x4240D4: CALL present_view, just after BRS2.RAW is loaded) ->
; CALL here: brr_splash_shown notes the start-up splash (splash.c), then present_view (0x426360),
; which returns to game_init. Every register kept (game_init has set ESI).
_brr_hook_splash_present:
        pushad
        call    _brr_splash_shown
        popad
        push    426360h
        ret

; Site frame_message (WndProc 0x401749: JZ 0x401972, the frame message 0x9000) -> JZ here.
; While brr_splash_hold says the start-up splash is held (splash.c), WndProc returns 0 (its
; exit at 0x4025E1) and no frame runs; otherwise on to the frame message as before (0x401972).
; Every register kept; POPAD leaves the flags alone.
_brr_hook_frame_message:
        pushad
        call    _brr_splash_hold
        test    eax, eax
        popad
        jnz     fm_hold
        push    401972h
        ret
fm_hold:
        push    4025E1h
        ret

_brr_hook_hud_icon:
        pushad
        push    0
        call    _brr_hud_begin
        add     esp, 4
        popad
        call    dword ptr [_brr_orig_hud_icon]
        pushad
        call    _brr_hud_end
        popad
        ret

; Site sprite_scaled_entry: sprite_draw_scaled's first instructions (0x426F90: PUSH ESI;
; PUSH EDI; PUSH EBP; SUB ESP,50h) -> JMP here. While the HUD draws, brr_hud_sprite(its
; caller, the bank in EAX, the index in DX, x in BX, y in CX) picks the element's layer and
; measures it; then those instructions and on from 0x426F96.
_brr_hook_sprite_scaled:
        cmp     dword ptr [_brr_hud_drawing], 0
        je      hss_continue
        pushad
        movsx   esi, cx
        push    esi                             ; y (640-wide units)
        movsx   esi, bx
        push    esi                             ; x
        movsx   esi, dx
        push    esi                             ; the sprite's index
        push    eax                             ; its bank
        push    dword ptr [esp + 48]            ; the return address (above PUSHAD and 4 pushes)
        call    _brr_hud_sprite
        add     esp, 20
        popad
hss_continue:
        push    esi
        push    edi
        push    ebp
        sub     esp, 50h
        jmp     dword ptr [_brr_sprite_scaled_continue]

; sprite_draw_scaled (0x426F90), the HUD's sprite routine, steps through the sprite with an
; error term starting at half the drawn size; drawn more than twice as big, the last row and
; column it reads are one past the sprite (bytes of whatever follows it), which Max's views
; wider than 1,280 pixels show. Sites sprite_scale_rows (0x42717A) and sprite_scale_columns
; (0x427199) call these instead: the start is max(drawn / 2, drawn - source), the game's own
; value up to twice the size, and never past the sprite's last pixel above it. EDX is free.
; Rows: drawn height in EBX, the sprite's height at [ESP+18h] in the caller (+4 here).
_brr_hook_sprite_rows:
        mov     eax, ebx
        cdq
        sub     eax, edx
        sar     eax, 1
        mov     edx, ebx
        sub     edx, dword ptr [esp + 1Ch]
        cmp     edx, eax
        jle     ssr_done
        mov     eax, edx
ssr_done:
        ret

; Columns: drawn width in ESI, the sprite's width at [ESP+30h] in the caller (+4 here).
_brr_hook_sprite_columns:
        mov     eax, esi
        cdq
        sub     eax, edx
        sar     eax, 1
        mov     edx, esi
        sub     edx, dword ptr [esp + 34h]
        cmp     edx, eax
        jle     ssc_done
        mov     eax, edx
ssc_done:
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
        test    eax, eax                        ; 0: the player's own texture distance
        jz      rv_texture_kept
        mov     dword ptr ds:[44B048h], eax
rv_texture_kept:
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
        mov     ebx, dword ptr [ecx*4 + _brr_reciprocals + 4]  ; the bigger table (recip_* sites)
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
        mov     ebx, dword ptr [ecx*4 + _brr_reciprocals + 4]  ; the bigger table (recip_* sites)
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
