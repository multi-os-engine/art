
.class public LCLASS_aax;
.super Ljava/lang/Object;
.implements LINTERFACE_aaw_DEFAULT;

# public class CLASS_aax implements INTERFACE_aaw_DEFAULT {
#   public String CalledClassName() {
#     return "[CLASS_aax [INTERFACE_aaw_DEFAULT [INTERFACE_aap ] [INTERFACE_aao ] [INTERFACE_aar [INTERFACE_aap ] [INTERFACE_aaq ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_aax [INTERFACE_aaw_DEFAULT [INTERFACE_aap ] [INTERFACE_aao ] [INTERFACE_aar [INTERFACE_aap ] [INTERFACE_aaq ]]]]"
  return-object v0
.end method

