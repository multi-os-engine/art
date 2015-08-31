
.class public LCLASS_aav;
.super Ljava/lang/Object;
.implements LINTERFACE_aau_DEFAULT;

# public class CLASS_aav implements INTERFACE_aau_DEFAULT {
#   public String CalledClassName() {
#     return "[CLASS_aav [INTERFACE_aau_DEFAULT [INTERFACE_aaq ] [INTERFACE_aao ] [INTERFACE_aar [INTERFACE_aap ] [INTERFACE_aaq ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_aav [INTERFACE_aau_DEFAULT [INTERFACE_aaq ] [INTERFACE_aao ] [INTERFACE_aar [INTERFACE_aap ] [INTERFACE_aaq ]]]]"
  return-object v0
.end method

