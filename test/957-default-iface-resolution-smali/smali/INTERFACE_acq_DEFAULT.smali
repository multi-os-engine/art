
.class public abstract interface LINTERFACE_acq_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_abt;

.implements LINTERFACE_abm;

.implements LINTERFACE_abv;


# public interface INTERFACE_acq_DEFAULT extends   INTERFACE_abt, INTERFACE_abm, INTERFACE_abv {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_acq_DEFAULT [INTERFACE_abt ] [INTERFACE_abm ] [INTERFACE_abv [INTERFACE_abt ] [INTERFACE_abu_DEFAULT ]]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_acq_DEFAULT [INTERFACE_abt ] [INTERFACE_abm ] [INTERFACE_abv [INTERFACE_abt ] [INTERFACE_abu_DEFAULT ]]]"
  return-object v0
.end method


# }

