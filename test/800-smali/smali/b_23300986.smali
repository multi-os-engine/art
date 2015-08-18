.class public LB23300986;

.super Ljava/lang/Object;

.method public static run(Ljava/lang/Object;)V
   .registers 3
   move-object v1, v2      # Copy parameter into v1.
   monitor-enter v2        # Lock on parameter
   monitor-exit v1         # Unlock on alias
   monitor-enter v2        # Do it again.
   monitor-exit v1
   return-void
.end method
