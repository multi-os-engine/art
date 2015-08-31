
.class public LCLASS_acp;
.super Ljava/lang/Object;
.implements LINTERFACE_aco_DEFAULT;

# public class CLASS_acp implements INTERFACE_aco_DEFAULT {
#   public String CalledClassName() {
#     return "[CLASS_acp [INTERFACE_aco_DEFAULT [INTERFACE_abu_DEFAULT ] [INTERFACE_abm ] [INTERFACE_abv [INTERFACE_abt ] [INTERFACE_abu_DEFAULT ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_acp [INTERFACE_aco_DEFAULT [INTERFACE_abu_DEFAULT ] [INTERFACE_abm ] [INTERFACE_abv [INTERFACE_abt ] [INTERFACE_abu_DEFAULT ]]]]"
  return-object v0
.end method

