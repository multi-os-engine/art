%default { "helper":"UndefinedInvokeHandler" }
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern $helper
    EXPORT_PC
    mov     x0, xSELF
    add     x1, xFP, #OFF_FP_SHADOWFRAME
    mov     x2, xPC
    mov     x3, xINST
    bl      $helper
    cmp     w0, #0
    beq     MterpException
    FETCH_ADVANCE_INST 3
    GET_INST_OPCODE ip
    GOTO_OPCODE ip

