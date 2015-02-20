.class public LTest;

.super Ljava/lang/Object;

.method public static run()V
   .registers 3
   new-instance v2, Ljava/lang/String;
   invoke-direct {v2}, Ljava/lang/String;-><init>()V
   const/4 v0, 0
   move v1, v0
   :start
   invoke-static {}, LMain;->blowup()V
   if-ne v1, v0, :end
   const/4 v2, 1
   invoke-static {v2}, Ljava/lang/Integer;->toString(I)Ljava/lang/String;
   move v2, v0
   # The call makes v2 float type.
   invoke-static {v2}, Ljava/lang/Float;->isNaN(F)Z
   const/4 v1, 1
   goto :start
   :end
   return-void
.end method
