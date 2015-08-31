.class public LA;
.super Ljava/lang/Object;
.implements LGreeter;

# class A implements Greeter {
#     public String SayHi() {
#         return "Hi ";
#     }
# }

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public SayHi()Ljava/lang/String;
    .registers 1

    const-string v0, "Hi "
    return-object v0
.end method
