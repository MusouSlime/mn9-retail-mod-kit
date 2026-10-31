/*
 * GFx UI fill stub for 4:3 / 16:10.
 *
 * UGFxMoviePlayer::SetViewport hardcodes the Scaleform movie's default
 * SM_ShowAll (letterbox) and the game never calls SetViewScaleMode.
 * This stub replaces the 7-byte
 *     movl $1, 0x64(%rax)
 * at 0x1a1b4ab and calls UGFxMoviePlayer::SetViewScaleMode(this, SM_NoBorder)
 * before returning to 0x1a1b4b2.
 *
 * SM_NoBorder = 3 fills the viewport and crops the overflowing edges;
 * SM_ExactFit = 2 would stretch instead.  Change the `$3` below for stretch.
 */

    .text
    .globl _start
_start:
    mov 0x60(%rbx), %rax
    movl $1, 0x64(%rax)
    mov %rbx, %rdi
    mov $3, %esi
    sub $8, %rsp
    call set_view_scale_mode
    add $8, %rsp
    ret

_end:

    .set set_view_scale_mode, 0x1a1b4d0
