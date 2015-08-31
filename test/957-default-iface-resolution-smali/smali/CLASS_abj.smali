
.class public LCLASS_abj;
.super Ljava/lang/Object;
.implements LINTERFACE_abi_DEFAULT;

# public class CLASS_abj implements INTERFACE_abi_DEFAULT {
#   public String CalledClassName() {
#     return "[CLASS_abj [INTERFACE_abi_DEFAULT [INTERFACE_abc ] [INTERFACE_aba_DEFAULT ] [INTERFACE_abd [INTERFACE_abb ] [INTERFACE_abc ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_abj [INTERFACE_abi_DEFAULT [INTERFACE_abc ] [INTERFACE_aba_DEFAULT ] [INTERFACE_abd [INTERFACE_abb ] [INTERFACE_abc ]]]]"
  return-object v0
.end method

