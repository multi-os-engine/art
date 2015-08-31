.class public LB;
.super Ljava/lang/Object;
.implements LGreeter2;

# class B implements Greeter2 {
#     public String SayHi() {
#         return "Hello ";
#     }
# }

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public SayHi()Ljava/lang/String;
    .registers 1

    const-string v0, "Hello "
    return-object v0
.end method
