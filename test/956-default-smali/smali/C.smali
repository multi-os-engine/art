.class public LC;
.super LA;

# class C extends A {
#     public String SayHiTwice() {
#         return "You don't control me";
#     }
# }

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, LA;-><init>()V
    return-void
.end method

.method public SayHiTwice()Ljava/lang/String;
    .registers 1

    const-string v0, "You don't control me"
    return-object v0
.end method
