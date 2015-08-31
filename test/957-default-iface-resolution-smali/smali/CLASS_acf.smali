
.class public LCLASS_acf;
.super Ljava/lang/Object;
.implements LINTERFACE_ace;

# public class CLASS_acf implements INTERFACE_ace {
#   public String CalledClassName() {
#     return "[CLASS_acf [INTERFACE_ace [INTERFACE_abm ] [INTERFACE_abs [INTERFACE_abq_DEFAULT ] [INTERFACE_abr ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_acf [INTERFACE_ace [INTERFACE_abm ] [INTERFACE_abs [INTERFACE_abq_DEFAULT ] [INTERFACE_abr ]]]]"
  return-object v0
.end method

