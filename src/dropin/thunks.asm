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
        extrn   _brr_orig_camera_update:dword
        extrn   _brr_fine_angles_active:dword
        extrn   _brr_rotation_continue:dword
        extrn   _brr_game_atan:dword
        extrn   _brr_before_present_window:near
        extrn   _brr_orig_present_window:dword

        public  _brr_TIMER_Init, _brr_TIMER_Shutdown, _brr_TIMER_Handler
        public  _brr_hook_clock_read, _brr_hook_retrace, _brr_hook_race_tick
        public  _brr_hook_race_draw, _brr_hook_camera_update, _brr_hook_rotation
        public  _brr_rotation_original, _brr_call_rotation_original
        public  _brr_call_game_atan, _brr_hook_cam4, _brr_hook_present_window

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

; camera_mode4: AX = [ESI+10h] as before, but "previous" ([ESI+46h]) is only
; updated when this frame ran at least one tick (frame_ticks, 44BF80h).
_brr_hook_cam4:
        mov     ax, word ptr [esi + 10h]
        cmp     byte ptr ds:[44BF80h], 0
        je      cam4_done
        mov     word ptr [esi + 46h], ax
cam4_done:
        ret

        end
