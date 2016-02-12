    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
#if MTERP_SUSPEND
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a2, a0)                       #  a2 <- vAA
    FETCH_S(a1, 1)                         #  a1 <- branch offset, in code units
    b${revcmp} a2, zero, 1f                #  branch to 1 if comparison failed
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a1, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
    bgez      a1, 3f
    lw        rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)  # refresh rtable base
3:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
#else
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a2, a0)                       #  a2 <- vAA
    FETCH_S(a1, 1)                         #  a1 <- branch offset, in code units
    b${revcmp} a2, zero, 1f                #  branch to 1 if comparison failed
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a1, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
    bgez      a1, 3f
    lw        ra, THREAD_FLAGS_OFFSET(rSELF)
    b         MterpCheckSuspendAndContinue
3:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
#endif
