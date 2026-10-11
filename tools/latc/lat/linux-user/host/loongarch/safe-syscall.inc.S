
    .global safe_syscall_base
    .global safe_syscall_start
    .global safe_syscall_end
    .type    safe_syscall_base,  @function
    .type    safe_syscall_start, @function
    .type    safe_syscall_end,   @function

    /*
     * This is the entry point for making a system call. The calling
     * convention here is that of a C varargs function with the
     * first argument an 'int *' to the signal_pending flag, the
     * second one the system call number (as a 'long'), and all further
     * arguments being syscall arguments (also 'long').
     * We return a long which is the syscall's return value, which
     * may be negative-errno on failure. Conversion to the
     * -1-and-errno-set convention is done by the calling wrapper.
     */
safe_syscall_base:
    .cfi_startproc
    /*
     * The syscall calling convention is nearly the same as C:
     * we enter with a0 == *signal_pending
     *               a1 == syscall number
     *               a2 ... a7 == syscall arguments
     *               and return the result in a0
     * and the syscall instruction needs
     *               a7 == syscall number
     *               a0 ... a5 == syscall arguments
     *               and returns the result in a0
     * Shuffle everything around appropriately.
     */
    addi.d      $r3, $r3, -8
    st.d        $r4, $r3, 0     /* signal_pending pointer */
    or          $r13, $r5, $r0  /*or  t1, a1, zero*/    /* syscall number */

    /* syscall arguments */
    or  $r4,  $r6,  $r0  /*or  a0, a2, zero*/
    or  $r5,  $r7,  $r0  /*or  a1, a3, zero*/
    or  $r6,  $r8,  $r0  /*or  a2, a4, zero*/
    or  $r7,  $r9,  $r0  /*or  a3, a5, zero*/
    or  $r8,  $r10, $r0  /*or  a4, a6, zero*/
    or  $r9,  $r11, $r0  /*or  a5, a7, zero*/
    or  $r11, $r13, $r0  /*or  a7, t1, zero*/

    /*
     * This next sequence of code works in conjunction with the
     * rewind_if_safe_syscall_function(). If a signal is taken
     * and the interrupted PC is anywhere between 'safe_syscall_start'
     * and 'safe_syscall_end' then we rewind it to 'safe_syscall_start'.
     * The code sequence must therefore be able to cope with this, and
     * the syscall instruction must be the final one in the sequence.
     */
safe_syscall_start:
    /* If signal_pending is non-zero, don't do the call */
    ld.d    $r12, $r3, 0
    ld.w    $r13, $r12, 0               /*ld.w    t1, t0, 0*/
    bne     $r13, $r0, 0f               /*bnez    t1, 0f*/
    syscall 0
safe_syscall_end:
    /* code path for having successfully executed the syscall */
    addi.d  $r3, $r3, 8
    jirl    $r0, $r1, 0                 /*jirl    zero, ra, 0*/

0:
    /* code path when we didn't execute the syscall */
    li.d    $r4, -TARGET_ERESTARTSYS    /*li    a0, -TARGET_ERESTARTSYS*/
    addi.d  $r3, $r3, 8
    jirl    $r0, $r1, 0                 /*jirl    zero, ra, 0*/
    .cfi_endproc

    .size    safe_syscall_base, .-safe_syscall_base
