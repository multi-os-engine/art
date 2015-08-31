
.class public LCLASS_aaj;
.super Ljava/lang/Object;
.implements LINTERFACE_aai;

# public class CLASS_aaj implements INTERFACE_aai {
#   public String CalledClassName() {
#     return "[CLASS_aaj [INTERFACE_aai [INTERFACE_aag_DEFAULT ] [INTERFACE_aah ]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_aaj [INTERFACE_aai [INTERFACE_aag_DEFAULT ] [INTERFACE_aah ]]]"
  return-object v0
.end method

