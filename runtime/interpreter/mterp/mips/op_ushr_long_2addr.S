    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* ushr-long/2addr vA, vB */
    GET_OPA4(t3)                           #  t3 <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a2, a3)                       #  a2 <- vB
    EAS2(t3, rFP, t3)                      #  t3 <- &fp[A]
    LOAD64(a0, a1, t3)                     #  a0/a1 <- vAA/vAA+1

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi      v0, a2, 0x20                 #  shift & 0x20
    srl       v1, a1, a2                   #  rhi<- ahi >> (shift&31)
    bnez      v0, .L${opcode}_finish
    srl       v0, a0, a2                   #  rlo<- alo >> (shift&31)
    not       a0, a2                       #  alo<- 31-n  (shift is 5b)
    sll       a1, 1
    sll       a1, a0                       #  ahi<- ahi << (32-(shift&31))
    or        v0, a1                       #  rlo<- rlo | ahi
    STORE64(v0, v1, t3)                    #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction
%break

.L${opcode}_finish:
    STORE64(v1, zero, t3)                  #  vAA/vAA+1 <- rlo/rhi
    GOTO_OPCODE(t0)                        #  jump to next instruction
