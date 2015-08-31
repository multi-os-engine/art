
.class public LCLASS_abl;
.super Ljava/lang/Object;
.implements LINTERFACE_abk_DEFAULT;

# public class CLASS_abl implements INTERFACE_abk_DEFAULT {
#   public String CalledClassName() {
#     return "[CLASS_abl [INTERFACE_abk_DEFAULT [INTERFACE_abb ] [INTERFACE_abc ] [INTERFACE_aba_DEFAULT ] [INTERFACE_abd [INTERFACE_abb ] [INTERFACE_abc ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_abl [INTERFACE_abk_DEFAULT [INTERFACE_abb ] [INTERFACE_abc ] [INTERFACE_aba_DEFAULT ] [INTERFACE_abd [INTERFACE_abb ] [INTERFACE_abc ]]]]"
  return-object v0
.end method

