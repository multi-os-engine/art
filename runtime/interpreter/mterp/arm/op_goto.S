    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    /* tuning: use sbfx for 6t2+ targets */
#if 0
    mov     r0, rINST, lsl #16          @ r0<- AAxx0000
    movs    rINST, r0, asr #24          @ rINST<- ssssssAA (sign-extended)
    EXPORT_PC
    mov     r0, rSELF
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    mov     r2, rINST
    bl      MterpProfileBranch          @ (self, shadow_frame, offset)
    cmp     r0, #0
    bne     MterpOnStackReplacement     @ Note: offset must be in rINST
    ldr     lr, [rSELF, #THREAD_FLAGS_OFFSET]
    adds    r2, rINST, rINST            @ r2<- byte offset, set flags
    FETCH_ADVANCE_INST_RB r2            @ update rPC, load rINST
       @ If backwards branch refresh rIBASE
    bmi     MterpCheckSuspendAndContinue
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction
#endif

    add     r1, rFP, #OFF_FP_SHADOWFRAME
    ldrhs   r2, [r1, #SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET]
    mov     r0, rINST, lsl #16          @ r0<- AAxx0000
    movs    rINST, r0, asr #24          @ rINST<- ssssssAA (sign-extended)

@ TODO: following becomes common code to footer.S for all branches if possible
@ setup: offset in rINST, shadowframe in r1, hotness count in r2

    bgt     .L_${opcode}_no_count       @ don't add forward branches to hotness
    cmp     r2, #0
    ble     .L_${opcode}_no_count       @ <= 0 means no countdown
    subs    r2, #1                      @ countdown
    strh    r2, [r1, #SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET]
    beq     .L_${opcode}_add_batch      @ counted down to zero - report
.L_${opcode}_no_count:
    cmp     r2, #JIT_CHECK_OSR          @ possible OSR re-entry?
    beq     .L_${opcode}_check_osr
.L_${opcode}_resume_branch:
    ldr     lr, [rSELF, #THREAD_FLAGS_OFFSET]
    adds    r2, rINST, rINST            @ r2<- byte offset, set flags
    FETCH_ADVANCE_INST_RB r2            @ update rPC, load rINST
       @ If backwards branch refresh rIBASE
@ TUNING: perhaps fold check suspend and hotness?  Why test/branch twice of < 0 offsets?
    bmi     MterpCheckSuspendAndContinue
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

.L_${opcode}_add_batch:
    ldr     r0, [rFP, #OFF_FP_METHOD]
    mov     r2, rSELF
    bl      MterpAddHotnessBatch        @ (method, shadow_frame, self)
    mov     r2, r0                      @ restore new hotness countdown to r2
    b       .L_${opcode}_no_count

.L_${opcode}_check_osr:
    mov     r0, rSELF
    mov     r2, rINST
    bl      MterpCheckForOSR            @ (self, shadow_frame, offset)
    bne     MterpOnStackReplacement
    b       .L_${opcode}_resume_branch





    EXPORT_PC
    mov     r0, rSELF
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    mov     r2, rINST
    bl      MterpProfileBranch          @ (self, shadow_frame, offset)
    cmp     r0, #0
    bne     MterpOnStackReplacement     @ Note: offset must be in rINST

    ldr     lr, [rSELF, #THREAD_FLAGS_OFFSET]
    adds    r2, rINST, rINST            @ r2<- byte offset, set flags
    FETCH_ADVANCE_INST_RB r2            @ update rPC, load rINST
       @ If backwards branch refresh rIBASE
    bmi     MterpCheckSuspendAndContinue
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction
