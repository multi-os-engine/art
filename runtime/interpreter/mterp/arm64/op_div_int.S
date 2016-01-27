%default {}
    /*
     * div-int
     *
     */
    FETCH w0, 1                         // w0<- CCBB
    lsr     w9, wINST, #8               // w9<- AA
    lsr     w3, w0, #8                  // w3<- CC
    and     w2, w0, #255                // w2<- BB
    GET_VREG w1, w3                     // w1<- vCC
    GET_VREG w0, w2                     // w0<- vBB
    cbz     w1, common_errDivideByZero
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    sdiv    w0, w0, w1                  // w0<- op
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG w0, w9                     // vAA<- w0
    GOTO_OPCODE ip                      // jump to next instruction
