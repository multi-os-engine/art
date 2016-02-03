%include "mips64/fcvtHeader.S" { "suffix":"_FLOAT", "valreg":"f0" }
    cvt.d.w f0, f0
%include "mips64/fcvtFooter.S" { "suffix":"_DOUBLE", "valreg":"f0" }
