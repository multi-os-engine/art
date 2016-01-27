    /*
     * Generic 32-bit floating point "/2addr" binary operation.  Provide
     * an "instr" line that specifies an instruction that performs
     * "s2 = s0 op s1".
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr, div-float/2addr
     */
    /* binop/2addr vA, vB */
    lsr     w3, wINST, #12              // w3<- B
    lsr     w9, wINST, #8               // w9<- A+
    VREG_INDEX_TO_ADDR x3, w3           // w3<- &vB
    and     w9, w9, #15                 // w9<- A
    ldr     s1, [x3]                    // s1<- vB
    VREG_INDEX_TO_ADDR x9, w9           // w9<- &vA
    FETCH_ADVANCE_INST 1                // advance rPC, load rINST
    ldr     s0, [x9]                    // s0<- vA
    $instr                              // s2<- op
    GET_INST_OPCODE ip                  // extract opcode from rINST
    str     s2, [x9]                    // vAA<- s2
    GOTO_OPCODE ip                      // jump to next instruction
