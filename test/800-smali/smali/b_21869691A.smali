# Test that the verifier does not stash methods incorrectly because they are being invoked with
# the wrong opcode.
#
# This is the class that calls B21869691B.a() with invoke-interface, which is incorrect.

.class public LB21869691A;

.super Ljava/lang/Object;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public run()V
  .registers 3
  new-instance v0, LB21869691C;
  invoke-direct {v0}, LB21869691C;-><init>()V
  invoke-virtual {v2, v0}, LB21869691A;->callinf(LB21869691C;)V
  return-void
.end method

.method public callinf(LB21869691C;)V
  .registers 2
  invoke-interface {p1}, LB21869691C;->a()V
  return-void
.end method
