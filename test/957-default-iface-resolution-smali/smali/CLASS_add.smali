
.class public LCLASS_add;
.super Ljava/lang/Object;
.implements LINTERFACE_adc;

# public class CLASS_add implements INTERFACE_adc {
#   public String CalledClassName() {
#     return "[CLASS_add [INTERFACE_adc [INTERFACE_acz_DEFAULT ] [INTERFACE_ada ] [INTERFACE_adb ]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_add [INTERFACE_adc [INTERFACE_acz_DEFAULT ] [INTERFACE_ada ] [INTERFACE_adb ]]]"
  return-object v0
.end method

