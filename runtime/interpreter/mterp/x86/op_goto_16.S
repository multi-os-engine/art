/*
 * Unconditional branch, 16-bit offset.
 *
 * The branch distance is a signed code-unit offset, which we need to
 * double to get a byte offset.
 */
    /* goto/16 +AAAA */
    movswl  2(rPC), rINST                   # eax <- ssssAAAA
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
    jg      1f                              # AA * 2 > 0 => no suspend check
#if MTERP_SUSPEND
    REFRESH_IBASE
#else
    jmp     MterpCheckSuspendAndContinue
#endif
1:
    GOTO_NEXT
