.class public LB20036522_2;
.super Ljava/lang/Object;

.method public static run(Ljava/lang/Object;I)V
.registers 3

    if-eqz p1, :else1

    monitor-enter p0
    monitor-enter p0

    goto :end1

:else1

    monitor-enter p0
    monitor-enter p0
    monitor-enter p0

:end1

    monitor-exit p0
    monitor-exit p0
    monitor-exit p0

    return-void

.end method