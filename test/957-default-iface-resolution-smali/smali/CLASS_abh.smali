
.class public LCLASS_abh;
.super Ljava/lang/Object;
.implements LINTERFACE_abg_DEFAULT;

# public class CLASS_abh implements INTERFACE_abg_DEFAULT {
#   public String CalledClassName() {
#     return "[CLASS_abh [INTERFACE_abg_DEFAULT [INTERFACE_abb ] [INTERFACE_aba_DEFAULT ] [INTERFACE_abd [INTERFACE_abb ] [INTERFACE_abc ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_abh [INTERFACE_abg_DEFAULT [INTERFACE_abb ] [INTERFACE_aba_DEFAULT ] [INTERFACE_abd [INTERFACE_abb ] [INTERFACE_abc ]]]]"
  return-object v0
.end method

