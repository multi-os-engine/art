    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto/16 +AAAA */
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    ldrhs   r2, [r1, #SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET]
    FETCH_S rINST, 1                    @ rINST<- ssssAAAA (sign-extended)
    b       MterpCommonTakenBranchNoFlags
