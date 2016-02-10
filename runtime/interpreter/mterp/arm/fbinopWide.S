    /*
     * Generic 64-bit double-precision floating point binary operation.
     * Provide an "instr" line that specifies an instruction that performs
     * "d2 = d0 op d1".
     *
     * for: add-double, sub-double, mul-double, div-double
     */
    /* doubleop vAA, vBB, vCC */
    FETCH r0, 1                         @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    VREG_INDEX_TO_ADDR r3, r3           @ r3<- &vCC
    VREG_INDEX_TO_ADDR r2, r2           @ r2<- &vBB
    fldd    d1, [r3]                    @ d1<- vCC
    fldd    d0, [r2]                    @ d0<- vBB
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    $instr                              @ s2<- op
    mov     ip, #0
    add     lr, r9, #1                  @ lr<- AA+1
    SET_VREG_SHADOW ip, r9              @ clear AA shadow
    SET_VREG_SHADOW ip, lr              @ clear AA+1 shadow
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    VREG_INDEX_TO_ADDR r9, r9           @ r9<- &vAA
    fstd    d2, [r9]                    @ vAA<- d2
    GOTO_OPCODE ip                      @ jump to next instruction
