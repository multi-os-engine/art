/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    EXPORT_PC()
    la     ra, artMterpAsmInstructionStart + (${opnum} * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    mov    a2, rPC
    la     a3, MterpCheckBefore
    jalr   zero, a3                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)
