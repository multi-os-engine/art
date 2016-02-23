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
    movl    rINST, %eax
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
    addl    rINST, rINST
    leal    (rPC, rINST), rPC
    FETCH_INST
    REFRESH_IBASE
    jg      1f
#if MTERP_SUSPEND
    #     REFRESH_IBASE - we did it above.
#else
    jmp     MterpCheckSuspendAndContinue
#endif
1:
    GOTO_NEXT
