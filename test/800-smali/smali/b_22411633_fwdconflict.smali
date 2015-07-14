.class public LB22411633FwdConflict;
.super Ljava/lang/Object;


.method public static run(Z)V
.registers 6
       # Make v3 & v4 defined, just use null.
       const v3, 0
       const v4, 0

       goto :Start

:LabelMerge
       # At this point, an initialized and an uninitialized reference are merged. However, the
       # merge is only from forward branches. If the conflict isn't used (as here), this should
       # pass the verifier.

       return-void

:Start
       # Allocate a java.lang.Object (do not initialize).
       new-instance v4, Ljava/lang/Object;

       # Branch forward.
       if-eqz v5, :LabelMerge

       # Create an initialized Object.
       new-instance v4, Ljava/lang/Object;
       invoke-direct {v4}, Ljava/lang/Object;-><init>()V

       # Just some random work.
       add-int/lit16 v3, v3, 1

       goto :LabelMerge

.end method
