/*
 * Unconditional branch, 32-bit offset.
 *
 * The branch distance is a signed code-unit offset, which we need to
 * double to get a byte offset.
 *
 * Unlike most opcodes, this one is allowed to branch to itself, so
 * our "backward branch" test must be "<=0" instead of "<0".  Because
 * we need the V bit set, we'll use an adds to convert from Dalvik
 * offset to byte offset.
 */
    /* goto/32 +AAAAAAAA */
    movl    2(rPC), rINST                   # rINST <- AAAAAAAA
#if MTERP_PROFILE_BRANCHES
    EXPORT_PC
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    leal    OFF_FP_SHADOWFRAME(rFP), %eax
    movl    %eax, OUT_ARG1(%esp)
    movl    rINST, OUT_ARG2(%esp)
    call    SYMBOL(MterpProfileBranch)
    testb   %al, %al
    jnz     MterpOnStackReplacement
    RESTORE_IBASE
#endif
    addl    rINST, rINST                    # rINST <- AA * 2
    leal    (rPC, rINST), rPC
    FETCH_INST
    jle     MterpCheckSuspendAndContinue    # AA * 2 <= 0 => suspend check
    GOTO_NEXT
