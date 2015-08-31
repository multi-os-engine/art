
.class public LCLASS_aab;
.super Ljava/lang/Object;
.implements LINTERFACE_aaa_DEFAULT;

# public class CLASS_aab implements INTERFACE_aaa_DEFAULT {
#   public String CalledClassName() {
#     return "[CLASS_aab [INTERFACE_aaa_DEFAULT ]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_aab [INTERFACE_aaa_DEFAULT ]]"
  return-object v0
.end method

