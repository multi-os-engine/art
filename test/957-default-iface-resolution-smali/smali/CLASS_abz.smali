
.class public LCLASS_abz;
.super Ljava/lang/Object;
.implements LINTERFACE_aby_DEFAULT;

# public class CLASS_abz implements INTERFACE_aby_DEFAULT {
#   public String CalledClassName() {
#     return "[CLASS_abz [INTERFACE_aby_DEFAULT [INTERFACE_abn ] [INTERFACE_abm ] [INTERFACE_abp_DEFAULT [INTERFACE_abn ] [INTERFACE_abo ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_abz [INTERFACE_aby_DEFAULT [INTERFACE_abn ] [INTERFACE_abm ] [INTERFACE_abp_DEFAULT [INTERFACE_abn ] [INTERFACE_abo ]]]]"
  return-object v0
.end method

