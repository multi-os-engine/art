.class public LCmpLong;
.super Ljava/lang/Object;


.method public constructor <init>()V
.registers 1
       invoke-direct {p0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

# This test is just here to make sure that we can at least print the non-lambda related works.
.method public static run()I
.registers 5000
    const-string v0, "SanityCheck"
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v1, v0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    const v2, 123456
    return v2
.end method
