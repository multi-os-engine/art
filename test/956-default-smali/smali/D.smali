.class public LD;
.super Ljava/lang/Object;
.implements LGreeter3;

# class D implements Greeter3 {
#     public String GetName() {
#         return "Alex ";
#     }
# }

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public GetName()Ljava/lang/String;
    .registers 1

    const-string v0, "Alex "
    return-object v0
.end method
