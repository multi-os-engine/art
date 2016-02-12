    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
#if MTERP_SUSPEND
    sll       a0, rINST, 16                #  a0 <- AAxx0000
    sra       a1, a0, 24                   #  a1 <- ssssssAA (sign-extended)
    addu      a2, a1, a1                   #  a2 <- byte offset
    /* If backwards branch refresh rIBASE */
    bgez      a1, 1f
    lw        rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)  # refresh handler base
1:
    FETCH_ADVANCE_INST_RB(a2)              #  update rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
#else
    sll       a0, rINST, 16                #  a0 <- AAxx0000
    sra       a1, a0, 24                   #  a1 <- ssssssAA (sign-extended)
    addu      a2, a1, a1                   #  a2 <- byte offset
    FETCH_ADVANCE_INST_RB(a2)              #  update rPC, load rINST
    /* If backwards branch refresh rIBASE */
    bgez      a1, 1f
    lw        ra, THREAD_FLAGS_OFFSET(rSELF)
    b         MterpCheckSuspendAndContinue
1:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
#endif
