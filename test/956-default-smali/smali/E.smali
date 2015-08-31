.class public LE;
.super Ljava/lang/Object;
.implements LGreeter;

# class E implements Greeter {
#     public String SayHi() {
#         return "Hi2 ";
#     }
# }

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public SayHi()Ljava/lang/String;
    .registers 1

    const-string v0, "Hi2 "
    return-object v0
.end method
