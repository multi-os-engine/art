.class public LB20036522;
.super Ljava/lang/Object;

.method public static run4(Ljava/lang/Object;II)V
.registers 3

       if-eqz p1, :else1
            monitor-enter p0
       :else1
            monitor-enter p0
       :end1

       if-eqz p2, :else2
            monitor-exit p0
       :else2
            monitor-exit p0
       :end2

       return-void

.end method



.method public static run5(Ljava/lang/Object;II)V
.registers 3

       if-eqz p2, :else2
            monitor-enter p0
       :else2
            monitor-enter p0
       :end2

       if-eqz p1, :else1
            monitor-exit p0
       :else1
            monitor-exit p0
       :end1

       return-void

.end method
