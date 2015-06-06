.class public LB17410612;

# Test that a hard + soft verifier failure in invoke-interface does not lead to
# an order abort (the last failure must be hard).

.super Ljava/lang/Object;

.method public run()V
    .registers 3
    const-wide v0, 0          # Make (v0, v1) a long
    invoke-static {v0, v2}, Ljava/lang/Long;->valueOf(J)Ljava/lang/Long;
    return-void
.end method
