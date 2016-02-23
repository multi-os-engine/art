/*
 * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
 * fragment that specifies the *reverse* comparison to perform, e.g.
 * for "if-le" you would use "gt".
 *
 * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
 */
    /* if-cmp vAA, +BBBB */
    cmpl    $$0, VREG_ADDRESS(rINST)        # compare (vA, 0)
    movl    $$2, rINST                      # assume branch not taken
    j${revcmp}   1f
    movswl  2(rPC),rINST                    # fetch signed displacement
1:
    EXPORT_PC
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    leal    OFF_FP_SHADOWFRAME(rFP), %eax
    movl    %eax, OUT_ARG1(%esp)
    movl    rINST, OUT_ARG2(%esp)
    call    SYMBOL(MterpProfileBranch)
    RESTORE_IBASE
    testb   %al, %al
    jnz     MterpOnStackReplacement
    addl    rINST, rINST                    # eax <- AA * 2
    leal    (rPC, rINST), rPC
    FETCH_INST
    jg      2f                              # AA * 2 > 0 => no suspend check
#if MTERP_SUSPEND
    REFRESH_IBASE
#else
    jmp     MterpCheckSuspendAndContinue
#endif
2:
    GOTO_NEXT
