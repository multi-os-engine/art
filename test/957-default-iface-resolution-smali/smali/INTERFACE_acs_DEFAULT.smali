
.class public abstract interface LINTERFACE_acs_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_abu_DEFAULT;

.implements LINTERFACE_abt;

.implements LINTERFACE_abm;

.implements LINTERFACE_abv;


# public interface INTERFACE_acs_DEFAULT extends   INTERFACE_abu_DEFAULT, INTERFACE_abt, INTERFACE_abm, INTERFACE_abv {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_acs_DEFAULT [INTERFACE_abu_DEFAULT ] [INTERFACE_abt ] [INTERFACE_abm ] [INTERFACE_abv [INTERFACE_abt ] [INTERFACE_abu_DEFAULT ]]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_acs_DEFAULT [INTERFACE_abu_DEFAULT ] [INTERFACE_abt ] [INTERFACE_abm ] [INTERFACE_abv [INTERFACE_abt ] [INTERFACE_abu_DEFAULT ]]]"
  return-object v0
.end method


# }

