.class public LG;
.super Ljava/lang/Object;
.implements LAttendant;

# class G implements Attendant {
#     public String GetPlace() {
#         return "android";
#     }
# }

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public GetPlace()Ljava/lang/String;
    .registers 1
    const-string v0, "android"
    return-object v0
.end method
