.class public LB29778499_2;
.super Ljava/lang/Object;

# Test invoking an interface method on an object that doesn't implement the interface.

.method public static run()V
.registers 1
       new-instance v0, Ljava/lang/Object;
       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       invoke-interface {v0}, Ljava/lang/Runnable;->run()V
       return-void
.end method
