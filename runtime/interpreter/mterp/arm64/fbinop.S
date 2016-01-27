    /*:
     * Generic 32-bit floating-point operation.
     *
     * For: add-float, sub-float, mul-float, div-float
     */
    /* floatop vAA, vBB, vCC */
    FETCH w0, 1                         // r0<- CCBB
    lsr     w9, wINST, #8               // r9<- AA
    lsr     w3, w0, #8                  // r3<- CC
    and     w2, w0, #255                // r2<- BB
    GET_VREG  s1, w3
    GET_VREG  s0, w2
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    $instr                              // s2<- op
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG  s2, w9
    GOTO_OPCODE ip                      // jump to next instruction
