
.class public abstract interface LINTERFACE_acc_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_abn;

.implements LINTERFACE_abo;

.implements LINTERFACE_abm;

.implements LINTERFACE_abp_DEFAULT;


# public interface INTERFACE_acc_DEFAULT extends   INTERFACE_abn, INTERFACE_abo, INTERFACE_abm, INTERFACE_abp_DEFAULT {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_acc_DEFAULT [INTERFACE_abn ] [INTERFACE_abo ] [INTERFACE_abm ] [INTERFACE_abp_DEFAULT [INTERFACE_abn ] [INTERFACE_abo ]]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_acc_DEFAULT [INTERFACE_abn ] [INTERFACE_abo ] [INTERFACE_abm ] [INTERFACE_abp_DEFAULT [INTERFACE_abn ] [INTERFACE_abo ]]]"
  return-object v0
.end method


# }

