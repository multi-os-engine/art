
.class public LCLASS_adi;
.super Ljava/lang/Object;
.implements LINTERFACE_adh;

# public class CLASS_adi implements INTERFACE_adh {
#   public String CalledClassName() {
#     return "[CLASS_adi [INTERFACE_adh [INTERFACE_ade ] [INTERFACE_adf_DEFAULT ] [INTERFACE_adg ]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_adi [INTERFACE_adh [INTERFACE_ade ] [INTERFACE_adf_DEFAULT ] [INTERFACE_adg ]]]"
  return-object v0
.end method

