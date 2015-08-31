.class public abstract interface LAttendant;
.super Ljava/lang/Object;

# public interface Attendant {
#     public default String SayHi() {
#         return "welcome to " + GetPlace();
#     }
#     public default String SayHiTwice() {
#         return SayHi() + SayHi();
#     }
#
#     public String GetPlace();
# }

.method public SayHi()Ljava/lang/String;
    .locals 2
    const-string v0, "welcome to "
    invoke-interface {p0}, LAttendant;->GetPlace()Ljava/lang/String;
    move-result-object v1
    invoke-virtual {v0, v1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method

.method public SayHiTwice()Ljava/lang/String;
    .locals 2
    invoke-interface {p0}, LAttendant;->SayHi()Ljava/lang/String;
    move-result-object v0
    invoke-interface {p0}, LAttendant;->SayHi()Ljava/lang/String;
    move-result-object v1
    invoke-virtual {v0, v1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method

.method public abstract GetPlace()Ljava/lang/String;
.end method
