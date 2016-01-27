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
    VREG_INDEX_TO_ADDR x3, w3           // r3<- &vCC
    VREG_INDEX_TO_ADDR x2, w2           // r2<- &vBB
    ldr     s1, [x3]                    // s1<- vCC
    ldr     s0, [x2]                    // s0<- vBB
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    $instr                              // s2<- op
    GET_INST_OPCODE ip                  // extract opcode from rINST
    VREG_INDEX_TO_ADDR x9, w9           // r9<- &vAA
    str     s2, [x9]                    // vAA<- s2
    GOTO_OPCODE ip                      // jump to next instruction
