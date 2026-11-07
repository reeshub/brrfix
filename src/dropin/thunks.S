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

# camera_mode4: AX = [ESI+0x10] as before, but "previous" ([ESI+0x46]) is only
# updated when this frame ran at least one tick.
        .globl  _brr_hook_cam4
_brr_hook_cam4:
        mov     ax, word ptr [esi + 0x10]
        cmp     byte ptr ds:0x44BF80, 0
        je      1f
        mov     word ptr [esi + 0x46], ax
1:      ret
