
.class public LCLASS_abx;
.super Ljava/lang/Object;
.implements LINTERFACE_abw;

# public class CLASS_abx implements INTERFACE_abw {
#   public String CalledClassName() {
#     return "[CLASS_abx [INTERFACE_abw [INTERFACE_abm ] [INTERFACE_abp_DEFAULT [INTERFACE_abn ] [INTERFACE_abo ]]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_abx [INTERFACE_abw [INTERFACE_abm ] [INTERFACE_abp_DEFAULT [INTERFACE_abn ] [INTERFACE_abo ]]]]"
  return-object v0
.end method

