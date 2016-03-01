    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    @ TUNING: rearrange register use to allow hoisting of hotness countdown
    mov     r0, rINST, lsr #8           @ r0<- AA
    GET_VREG r2, r0                     @ r2<- vAA
    FETCH_S rINST, 1                    @ rINST<- branch offset, in code units
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    cmp     r2, #0                      @ compare (vA, 0)
    ldrhs   r2, [r1, #SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET]
    @ TUNING: use non-reversed comparison and handle not taken case in line.
    b${revcmp} MterpCommonNotTakenBranch
    b          MterpCommonTakenBranchNoFlags
