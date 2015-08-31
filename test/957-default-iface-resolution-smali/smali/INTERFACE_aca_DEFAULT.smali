
.class public abstract interface LINTERFACE_aca_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_abo;

.implements LINTERFACE_abm;

.implements LINTERFACE_abp_DEFAULT;


# public interface INTERFACE_aca_DEFAULT extends   INTERFACE_abo, INTERFACE_abm, INTERFACE_abp_DEFAULT {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_aca_DEFAULT [INTERFACE_abo ] [INTERFACE_abm ] [INTERFACE_abp_DEFAULT [INTERFACE_abn ] [INTERFACE_abo ]]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_aca_DEFAULT [INTERFACE_abo ] [INTERFACE_abm ] [INTERFACE_abp_DEFAULT [INTERFACE_abn ] [INTERFACE_abo ]]]"
  return-object v0
.end method


# }

