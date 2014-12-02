.class public LInvokeDirect;
.super LInvokeDirectSuper;

.method public constructor <init>()V
      .registers 2
       invoke-direct {v1}, LInvokeDirectSuper;-><init>()V
       return-void
.end method

.method public run()I
       .registers 3
       invoke-super {v2}, LInvokeDirectSuper;->privateMethod()I
       move-result v0
       return v0
.end method
