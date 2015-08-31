
.class public LCLASS_adn;
.super Ljava/lang/Object;
.implements LINTERFACE_adm;

# public class CLASS_adn implements INTERFACE_adm {
#   public String CalledClassName() {
#     return "[CLASS_adn [INTERFACE_adm [INTERFACE_adj ] [INTERFACE_adk ] [INTERFACE_adl_DEFAULT ]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_adn [INTERFACE_adm [INTERFACE_adj ] [INTERFACE_adk ] [INTERFACE_adl_DEFAULT ]]]"
  return-object v0
.end method

