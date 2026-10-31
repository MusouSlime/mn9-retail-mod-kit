/*
 * Force Scaleform's movie view scale mode to SM_NoBorder (3) whenever the
 * movie viewport is set.  UGFxMoviePlayer::SetViewport only sets the viewport
 * rectangle and leaves the movie's scale mode at the Scaleform default
 * (SM_ShowAll = letterbox), so 4:3 / 16:10 windows show 16:9 black bars.
 *
 * The 5-byte instruction pair at 0x201d251
 *     mov %rdi,%r15 ; lea 0xa0(%r15),%r14   (first 2 bytes)
 * is replaced by a jump here.  This stub reproduces those instructions, sets
 * MovieImpl+0xe8 = 3, and jumps back to 0x201d25b.
 *
 * SM_ExactFit stretches the 16:9 UI to fill the window without cropping.
 * For NoBorder (fill + crop) change `$2` to `$3`.
 */

    .text
    .globl _start
_start:
    mov %rdi, %r15
    lea 0xa0(%r15), %r14
    movl $2, 0xe8(%r15)
    jmp back

_end:

    .set back, 0x201d25b
