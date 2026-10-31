/*
 * Code cave for the MN9 Cursor Hold ExternalInterface callbacks.
 *
 * Scaleform only forwards AS ExternalInterface.call() names that exist as
 * UFunctions on the movie player.  Free-form names such as
 * "Mn9FreeMouseOn" are dropped before FGFxExternalInterface::Callback().
 * We therefore reuse two harmless UScript->AS wrapper names that are
 * registered on MN9GFxMoviePlayer:
 *
 *   ASSetText             -> Cursor Hold ON:
 *                            SDL_SetWindowGrab(g_sdlWindow, 1)
 *                            SDL_ShowCursor(SDL_DISABLE)
 *   ASInputSetInputEnable -> Cursor Hold OFF:
 *                            SDL_SetWindowGrab(g_sdlWindow, 0)
 *                            SDL_ShowCursor(SDL_ENABLE)
 *
 * The stack must be 16-byte aligned before every call.  Without the `sub $8`
 * below, SDL_SetWindowGrab() crashes inside XSync() (SIGSEGV in
 * xcb_wait_for_reply), because the disassembler-visible call site is not
 * ABI-aligned.
 *
 * The cave replaces UObject::FindFunction at 0x1b24387 inside
 * FGFxExternalInterface::Callback.  At that point:
 *   rdi = target UObject, rsi = FName, r15 = AS callback name (char*),
 *   r13 = args, r14d = arg count.
 *
 * Any other name falls through to the original FindFunction.  Handled names
 * return rax=0 so the dispatcher takes its "function not found" path; the AS
 * return value is unused.
 */

    .text
    .globl _start
_start:
    push %rdi
    push %rsi
    sub $8, %rsp

    mov %r15, %rdi
    lea name_on(%rip), %rsi
    call strcmp
    test %eax, %eax
    je .on

    mov %r15, %rdi
    lea name_off(%rip), %rsi
    call strcmp
    test %eax, %eax
    je .off

    add $8, %rsp
    pop %rsi
    pop %rdi
    jmp find_function

.on:
    add $16, %rsp
    lea msg_on(%rip), %rdi
    xor %eax, %eax
    call printf
    xor %edi, %edi
    call fflush
    mov g_sdl_window(%rip), %rdi
    mov $1, %esi
    call sdl_set_window_grab
    xor %edi, %edi
    call sdl_show_cursor
    add $8, %rsp
    xor %eax, %eax
    ret

.off:
    add $16, %rsp
    lea msg_off(%rip), %rdi
    xor %eax, %eax
    call printf
    xor %edi, %edi
    call fflush
    mov g_sdl_window(%rip), %rdi
    xor %esi, %esi
    call sdl_set_window_grab
    mov $1, %edi
    call sdl_show_cursor
    add $8, %rsp
    xor %eax, %eax
    ret

name_on:
    .asciz "ASSetText"
name_off:
    .asciz "ASInputSetInputEnable"
msg_on:
    .asciz "[mn9] Free Mouse: on (cursor visible)\n"
msg_off:
    .asciz "[mn9] Free Mouse: off (cursor hidden)\n"

_end:

    .set strcmp,             0xccfec0
    .set find_function,      0xdd9bf0
    .set sdl_set_window_grab, 0xccfc30
    .set sdl_show_cursor,    0xccfef0
    .set g_sdl_window,       0x2f09608
    .set printf,             0xccf530
    .set fflush,             0xcced10
