.class public LF;
.super LA;
.implements LAttendant;

# class F extends A implements Attendant {
#     public String GetPlace() {
#         return "android";
#     }
#     public String SayHiTwice() {
#         return "We can override both interfaces";
#     }
# }

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public SayHiTwice()Ljava/lang/String;
    .registers 1

    const-string v0, "We can override both interfaces"
    return-object v0
.end method

.method public GetPlace()Ljava/lang/String;
    .registers 1
    const-string v0, "android"
    return-object v0
.end method
