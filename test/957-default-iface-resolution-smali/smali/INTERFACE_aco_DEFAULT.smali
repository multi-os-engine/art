
.class public abstract interface LINTERFACE_aco_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_abu_DEFAULT;

.implements LINTERFACE_abm;

.implements LINTERFACE_abv;


# public interface INTERFACE_aco_DEFAULT extends   INTERFACE_abu_DEFAULT, INTERFACE_abm, INTERFACE_abv {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_aco_DEFAULT [INTERFACE_abu_DEFAULT ] [INTERFACE_abm ] [INTERFACE_abv [INTERFACE_abt ] [INTERFACE_abu_DEFAULT ]]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_aco_DEFAULT [INTERFACE_abu_DEFAULT ] [INTERFACE_abm ] [INTERFACE_abv [INTERFACE_abt ] [INTERFACE_abu_DEFAULT ]]]"
  return-object v0
.end method


# }

