.class public LMain;

.super Ljava/lang/Object;

.method public static main([Ljava/lang/String;)V
    .registers 2

    invoke-static {}, LSanityCheck;->run()I
    invoke-static {}, LTrivialHelloWorld;->run()V

# TODO: add tests when verification fails

    return-void
.end method
