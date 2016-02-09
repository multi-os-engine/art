    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto/16 +AAAA */
#if MTERP_SUSPEND
    FETCH_S(a0, 1)                         #  a0 <- ssssAAAA (sign-extended)
    addu      a1, a0, a0                   #  a1 <- byte offset, flags set
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
    bgez      a1, 1f
    lw        rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)  # refresh handler base
1:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
#else
    FETCH_S(a0, 1)                         #  a0 <- ssssAAAA (sign-extended)
    addu      a1, a0, a0                   #  a1 <- byte offset, flags set
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
    bgez      a1, 1f
    lw        ra, THREAD_FLAGS_OFFSET(rSELF)
    b         MterpCheckSuspendAndContinue
1:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
#endif
