
.class public LCLASS_aaf;
.super Ljava/lang/Object;
.implements LINTERFACE_aae_DEFAULT;

# public class CLASS_aaf implements INTERFACE_aae_DEFAULT {
#   public String CalledClassName() {
#     return "[CLASS_aaf [INTERFACE_aae_DEFAULT [INTERFACE_aac ] [INTERFACE_aad ]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_aaf [INTERFACE_aae_DEFAULT [INTERFACE_aac ] [INTERFACE_aad ]]]"
  return-object v0
.end method

