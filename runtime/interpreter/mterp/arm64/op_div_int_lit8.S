%default {}
    /*
     * div-int/lit8
     *
     */
    FETCH_S w3, 1                       // w3<- ssssCCBB (sign-extended for CC
    lsr     w9, wINST, #8               // w9<- AA
    and     w2, w3, #255                // w2<- BB
    GET_VREG w0, w2                     // w0<- vBB
    asr     w1, w3, #8                  // w1<- ssssssCC (sign extended)
    cbz     w1, common_errDivideByZero
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    sdiv    w0, w0, w1                  // w0<- op
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG w0, w9                     // vAA<- w0
    GOTO_OPCODE ip                      // jump to next instruction
    /* 10-12 instructions */
