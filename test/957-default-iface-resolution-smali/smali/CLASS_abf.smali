
.class public LCLASS_abf;
.super Ljava/lang/Object;
.implements LINTERFACE_abe;

# public class CLASS_abf implements INTERFACE_abe {
#   public String CalledClassName() {
#     return "[CLASS_abf [INTERFACE_abe [INTERFACE_aba_DEFAULT ] [INTERFACE_abd [INTERFACE_abb ] [INTERFACE_abc ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_abf [INTERFACE_abe [INTERFACE_aba_DEFAULT ] [INTERFACE_abd [INTERFACE_abb ] [INTERFACE_abc ]]]]"
  return-object v0
.end method

