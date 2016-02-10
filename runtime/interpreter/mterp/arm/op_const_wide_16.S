    /* const-wide/16 vAA, #+BBBB */
    FETCH_S r0, 1                       @ r0<- ssssBBBB (sign-extended
    mov     r3, rINST, lsr #8           @ r3<- AA
    mov     r1, r0, asr #31             @ r1<- ssssssss
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    mov     r2, #0
    add     lr, r3, #1                  @ r3<- AA+1
    SET_VREG_SHADOW r2, r3              @ clear AA shadow
    SET_VREG_SHADOW r2, lr              @ clear AA+1 shadow
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[AA]
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    stmia   r3, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE ip                      @ jump to next instruction
