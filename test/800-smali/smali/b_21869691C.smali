# Test that the verifier does not stash methods incorrectly because they are being invoked with
# the wrong opcode.
#
# This is the non-interface class that implements the interface. We need a subclass of this, so
# that the implementation or miranda method isn't found in the subclass.
#
# We also use this to have a correct call.

.class public LB21869691C;

.super LB21869691B;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, LB21869691B;-><init>()V
    return-void
.end method
