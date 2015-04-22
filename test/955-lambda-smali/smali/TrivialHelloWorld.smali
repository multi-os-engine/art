.class public LTrivialHelloWorld;
.super Ljava/lang/Object;

.method public constructor <init>()V
.registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public static run()V
.registers 6
    # Trivial 0-arg hello world
    create-lambda v0, LTrivialHelloWorld;->doHelloWorld(Ljava/lang/reflect/ArtMethod;)V
    invoke-lambda v0, {}

    # Slightly more interesting 4-arg hello world
    create-lambda v1, doHelloWorldArgs(Ljava/lang/reflect/ArtMethod;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v2, "A"
    const-string v3, "B"
    const-string v4, "C"
    const-string v5, "D"
    invoke-lambda v1, {v2, v3, v4, v5}
    
    return-void
.end method

#TODO: should use a closure type instead of ArtMethod.
.method public static doHelloWorld(Ljava/lang/reflect/ArtMethod;)V
    .registers 3 # 1 parameters, 2 locals

    const-string v0, "Hello world! (0-args, no closure)"

    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v1, v0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    return-void
.end method

#TODO: should use a closure type instead of ArtMethod.
.method public static doHelloWorldArgs(Ljava/lang/reflect/ArtMethod;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    .registers 7 # 5 parameters, 2 locals

    const-string v0, " Hello world! (4-args, no closure)"
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;

    invoke-virtual {v1, p1}, Ljava/io/PrintStream;->print(Ljava/lang/String;)V
    invoke-virtual {v1, p2}, Ljava/io/PrintStream;->print(Ljava/lang/String;)V
    invoke-virtual {v1, p3}, Ljava/io/PrintStream;->print(Ljava/lang/String;)V
    invoke-virtual {v1, p4}, Ljava/io/PrintStream;->print(Ljava/lang/String;)V

    invoke-virtual {v1, v0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    return-void
.end method
