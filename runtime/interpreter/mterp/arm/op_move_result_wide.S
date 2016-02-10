    /* move-result-wide vAA */
    mov     rINST, rINST, lsr #8        @ rINST<- AA
    ldr     r3, [rFP, #OFF_FP_RESULT_REGISTER]
    add     r2, rFP, rINST, lsl #2      @ r2<- &fp[AA]
    ldmia   r3, {r0-r1}                 @ r0/r1<- retval.j
    mov     ip, #0
    add     lr, rINST, #1               @ lr<- AA+1
    SET_VREG_SHADOW ip, rINST           @ clear AA shadow
    SET_VREG_SHADOW ip, lr              @ clear AA+1 shadow
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST
    stmia   r2, {r0-r1}                 @ fp[AA]<- r0/r1
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction
