
.class public LCLASS_acr;
.super Ljava/lang/Object;
.implements LINTERFACE_acq_DEFAULT;

# public class CLASS_acr implements INTERFACE_acq_DEFAULT {
#   public String CalledClassName() {
#     return "[CLASS_acr [INTERFACE_acq_DEFAULT [INTERFACE_abt ] [INTERFACE_abm ] [INTERFACE_abv [INTERFACE_abt ] [INTERFACE_abu_DEFAULT ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_acr [INTERFACE_acq_DEFAULT [INTERFACE_abt ] [INTERFACE_abm ] [INTERFACE_abv [INTERFACE_abt ] [INTERFACE_abu_DEFAULT ]]]]"
  return-object v0
.end method

