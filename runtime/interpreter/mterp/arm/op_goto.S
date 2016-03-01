    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    ldrhs   r2, [r1, #SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET]
    sbfx    rINST, rINST, #8, #8           @ rINST<- ssssssAA (sign-extended)
    b       MterpCommonTakenBranchNoFlags  @ (r1:shadow_frame, rINST:offset, flags sets)
