# Register-preserving entry points between the game's Watcom code and our C code.
# Watcom callers expect every register except EAX to survive a call; our C code
# (cdecl) only preserves EBX, ESI, EDI and EBP, so each thunk saves the rest.
        .intel_syntax noprefix
        .text

# TIMER.DLL exports (see timer.def). TIMER_Init_ takes hInstance in EAX.
        .globl  _brr_TIMER_Init
_brr_TIMER_Init:
        push    ecx
        push    edx
        push    eax
        call    _brr_timer_init
        add     esp, 4
        pop     edx
        pop     ecx
        ret

        .globl  _brr_TIMER_Shutdown
_brr_TIMER_Shutdown:
        push    ecx
        push    edx
        call    _brr_timer_shutdown
        pop     edx
        pop     ecx
        ret

# The original's exported multimedia-timer callback (__stdcall, 5 arguments).
        .globl  _brr_TIMER_Handler
_brr_TIMER_Handler:
        lock inc dword ptr [_brr_timer_count]
        ret     20

# Patch targets.
        .globl  _brr_hook_clock_read
_brr_hook_clock_read:              # returns the count in EAX (the game uses AX)
        push    ecx
        push    edx
        call    _brr_clock_read
        pop     edx
        pop     ecx
        ret

        .globl  _brr_hook_retrace
_brr_hook_retrace:                 # reached by JMP from vga_wait_retrace
        pushad
        call    _brr_retrace
        popad
        ret

        .globl  _brr_hook_race_tick
_brr_hook_race_tick:
        pushad
        call    _brr_before_tick
        popad
        call    dword ptr [_brr_orig_race_tick]
        pushad
        call    _brr_after_tick
        popad
        ret

        .globl  _brr_hook_race_draw
_brr_hook_race_draw:
        pushad
        call    _brr_before_draw
        popad
        call    dword ptr [_brr_orig_race_draw]
        pushad
        call    _brr_after_draw
        popad
        ret

# render_view's call to camera_update (record in EAX; its result in EAX is used).
        .globl  _brr_hook_camera_update
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

# FUN_00446670 (the 3D engine's rotation-matrix builder) is entered by a JMP to here.
# EAX pitch, EBX yaw, ECX roll (low 16 bits), EBP the 3x3 matrix to fill. During the race
# draw with fine angles on, our C version builds it; otherwise the original runs.
        .globl  _brr_hook_rotation
_brr_hook_rotation:
        cmp     dword ptr [_brr_fine_angles_active], 0
        je      _brr_rotation_original
        push    edi
        push    esi
        sub     esp, 16                 # regs[4]: what the original leaves in EAX..EDX
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

# The original routine: its first three instructions (the five bytes the JMP replaced),
# then the rest of it in place.
        .globl  _brr_rotation_original
_brr_rotation_original:
        push    edi
        push    esi
        shr     eax, 5
        jmp     dword ptr [_brr_rotation_continue]

# void brr_call_rotation_original(pitch, yaw, roll, int32_t *m, uint32_t *regs): for the proof
        .globl  _brr_call_rotation_original
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

# int brr_call_game_atan(int y, int x): the game's FUN_00412a20 (EAX = y, EDX = x)
        .globl  _brr_call_game_atan
_brr_call_game_atan:
        push    ebx
        mov     eax, [esp + 8]
        mov     edx, [esp + 12]
        call    dword ptr [_brr_game_atan]
        pop     ebx
        ret

# camera_mode4: AX = [ESI+0x10] as before, but "previous" ([ESI+0x46]) is only
# updated when this frame ran at least one tick.
        .globl  _brr_hook_cam4
_brr_hook_cam4:
        mov     ax, word ptr [esi + 0x10]
        cmp     byte ptr ds:0x44BF80, 0
        je      1f
        mov     word ptr [esi + 0x46], ax
1:      ret
