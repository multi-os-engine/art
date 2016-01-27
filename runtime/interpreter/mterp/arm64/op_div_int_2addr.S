%default {}
    /*
     * Specialized 32-bit binary operation
     *
     * Performs "w0 = w0 div w1". The selection between sdiv or the gcc helper
     * depends on the compile time value of __ARM_ARCH_EXT_IDIV__ (defined for
     * ARMv7 CPUs that have hardware division support).
     *
     * div-int/2addr
     *
     */
    lsr     w3, wINST, #12              // w3<- B
    ubfx    w9, wINST, #8, #4           // w9<- A
    GET_VREG w1, w3                     // w1<- vB
    GET_VREG w0, w9                     // w0<- vA
    cbz     w1, common_errDivideByZero
    FETCH_ADVANCE_INST 1                // advance rPC, load rINST
    sdiv    w0, w0, w1                  // w0<- op
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG w0, w9                     // vAA<- w0
    GOTO_OPCODE ip                      // jump to next instruction
    /* 10-13 instructions */

