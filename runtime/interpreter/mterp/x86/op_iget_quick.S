%default { "load":"movl", "is_object":"0" }
    /* For: iget-quick, iget-object-quick */
    /* op vA, vB, offset@CCCC */
    movzbl  rINSTbl, %ecx                   # ecx <- BA
    sarl    $$4, %ecx                       # ecx <- B
    GET_VREG %ecx %ecx                      # vB (object we're operating on)
    movzwl  2(rPC), %eax                    # eax <- field byte offset
    testl   %ecx, %ecx                      # is object null?
    je      common_errNullObject
    ${load} (%ecx,%eax,1), %eax
    andb    $$0xf,rINSTbl                   # rINST <- A
    .if $is_object
    SET_VREG_OBJECT %eax rINST              # fp[A] <- value
    .else
    SET_VREG %eax rINST                     # fp[A] <- value
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2
