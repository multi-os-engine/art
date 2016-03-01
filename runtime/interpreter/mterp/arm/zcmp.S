    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    mov     r0, rINST, lsr #8           @ r0<- AA
    GET_VREG r0, r0                     @ r0<- vAA
    FETCH_S rINST, 1                    @ rINST<- branch offset, in code units
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    ldrhs   r2, [r1, #SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET]
    cmp     r0, #0                      @ compare (vA, 0)
    b${cmpcond} MterpCommonTakenBranchNoFlags
    cmp     r2, #JIT_CHECK_OSR          @ possible OSR re-entry?
    beq     .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction
