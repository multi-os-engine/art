%default { "func":"MterpDoPackedSwitch" }
/*
 * Handle a packed-switch or sparse-switch instruction.  In both cases
 * we decode it and hand it off to a helper function.
 *
 * We don't really expect backward branches in a switch statement, but
 * they're perfectly legal, so we check for them here.
 *
 * for: packed-switch, sparse-switch
 */
    /* op vAA, +BBBB */
    movl    2(rPC), %ecx                    # ecx <- BBBBbbbb
    GET_VREG %eax, rINST                    # eax <- vAA
    leal    (rPC,%ecx,2), %ecx              # ecx <- PC + BBBBbbbb*2
    movl    %eax, OUT_ARG1(%esp)            # ARG1 <- vAA
    movl    %ecx, OUT_ARG0(%esp)            # ARG0 <- switchData
    call    SYMBOL($func)
    movl    %eax, rINST
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
    addl    rINST, rINST
    leal    (rPC, rINST), rPC
    FETCH_INST
    REFRESH_IBASE
    jle     MterpCheckSuspendAndContinue
    GOTO_NEXT
