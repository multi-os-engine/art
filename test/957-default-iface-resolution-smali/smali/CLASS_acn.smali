
.class public LCLASS_acn;
.super Ljava/lang/Object;
.implements LINTERFACE_acm;

# public class CLASS_acn implements INTERFACE_acm {
#   public String CalledClassName() {
#     return "[CLASS_acn [INTERFACE_acm [INTERFACE_abm ] [INTERFACE_abv [INTERFACE_abt ] [INTERFACE_abu_DEFAULT ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_acn [INTERFACE_acm [INTERFACE_abm ] [INTERFACE_abv [INTERFACE_abt ] [INTERFACE_abu_DEFAULT ]]]]"
  return-object v0
.end method

