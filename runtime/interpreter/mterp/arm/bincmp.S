    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    @ TUNING: rearrange register use to allow hoisting of hotness countdown
    mov     r1, rINST, lsr #12          @ r1<- B
    ubfx    r0, rINST, #8, #4           @ r0<- A
    GET_VREG r3, r1                     @ r3<- vB
    GET_VREG r2, r0                     @ r2<- vA
    FETCH_S rINST, 1                    @ rINST<- branch offset, in code units
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    cmp     r2, r3                      @ compare (vA, vB)
    ldrhs   r2, [r1, #SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET]
    @ TUNING: use non-reversed comparison and handle not taken case in line.
    b${revcmp} MterpCommonNotTakenBranch
    b          MterpCommonTakenBranchNoFlags
