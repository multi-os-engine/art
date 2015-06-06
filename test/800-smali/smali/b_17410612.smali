.class public LB17410612;

# Test that an invoke with a long parameter has the long parameter in
# a pair. This should fail in the verifier and not an abort in the compiler.

.super Ljava/lang/Object;

.method public run()V
    .registers 3
    const-wide v0, 0          # Make (v0, v1) a long
    invoke-static {v0, v2}, Ljava/lang/Long;->valueOf(J)Ljava/lang/Long;
    return-void
.end method
