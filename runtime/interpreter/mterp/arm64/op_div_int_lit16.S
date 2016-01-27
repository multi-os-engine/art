%default {}
    /*
     * div-int/lit16
     *
     */
    FETCH_S w1, 1                       // w1<- ssssCCCC (sign-extended)
    lsr     w2, wINST, #12              // w2<- B
    ubfx    w9, wINST, #8, #4           // w9<- A
    GET_VREG w0, w2                     // w0<- vB
    cbz     w1, common_errDivideByZero
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    sdiv    w0, w0, w1                  // w0<- op
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG w0, w9                     // vAA<- w0
    GOTO_OPCODE ip                      // jump to next instruction
    /* 10-13 instructions */
