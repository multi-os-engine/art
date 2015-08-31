
.class public LCLASS_aan;
.super Ljava/lang/Object;
.implements LINTERFACE_aam;

# public class CLASS_aan implements INTERFACE_aam {
#   public String CalledClassName() {
#     return "[CLASS_aan [INTERFACE_aam [INTERFACE_aak ] [INTERFACE_aal_DEFAULT ]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_aan [INTERFACE_aam [INTERFACE_aak ] [INTERFACE_aal_DEFAULT ]]]"
  return-object v0
.end method

