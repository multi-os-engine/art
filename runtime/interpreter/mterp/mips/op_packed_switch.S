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
#if MTERP_SUSPEND
    FETCH(a0, 1)                           #  a0 <- bbbb (lo)
    FETCH(a1, 2)                           #  a1 <- BBBB (hi)
    GET_OPA(a3)                            #  a3 <- AA
    sll       t0, a1, 16
    or        a0, a0, t0                   #  a0 <- BBBBbbbb
    GET_VREG(a1, a3)                       #  a1 <- vAA
    EAS1(a0, rPC, a0)                      #  a0 <- PC + BBBBbbbb*2
    JAL($func)                             #  a0 <- code-unit branch offset
    addu      a1, v0, v0                   #  a1 <- byte offset
    bgtz      a1, 1f
    lw        rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF) #  refresh handler base
1:
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
#else
    FETCH(a0, 1)                           #  a0 <- bbbb (lo)
    FETCH(a1, 2)                           #  a1 <- BBBB (hi)
    GET_OPA(a3)                            #  a3 <- AA
    sll       t0, a1, 16
    or        a0, a0, t0                   #  a0 <- BBBBbbbb
    GET_VREG(a1, a3)                       #  a1 <- vAA
    EAS1(a0, rPC, a0)                      #  a0 <- PC + BBBBbbbb*2
    JAL($func)                             #  a0 <- code-unit branch offset
    addu      a1, v0, v0                   #  a1 <- byte offset
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
    bgtz      a1, 1f
    lw        ra, THREAD_FLAGS_OFFSET(rSELF)
    b         MterpCheckSuspendAndContinue
1:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
#endif
