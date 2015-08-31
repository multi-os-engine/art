
.class public LCLASS_aaz;
.super Ljava/lang/Object;
.implements LINTERFACE_aay_DEFAULT;

# public class CLASS_aaz implements INTERFACE_aay_DEFAULT {
#   public String CalledClassName() {
#     return "[CLASS_aaz [INTERFACE_aay_DEFAULT [INTERFACE_aaq ] [INTERFACE_aap ] [INTERFACE_aao ] [INTERFACE_aar [INTERFACE_aap ] [INTERFACE_aaq ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_aaz [INTERFACE_aay_DEFAULT [INTERFACE_aaq ] [INTERFACE_aap ] [INTERFACE_aao ] [INTERFACE_aar [INTERFACE_aap ] [INTERFACE_aaq ]]]]"
  return-object v0
.end method

