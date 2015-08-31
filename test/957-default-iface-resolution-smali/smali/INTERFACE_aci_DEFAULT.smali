
.class public abstract interface LINTERFACE_aci_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_abq_DEFAULT;

.implements LINTERFACE_abm;

.implements LINTERFACE_abs;


# public interface INTERFACE_aci_DEFAULT extends   INTERFACE_abq_DEFAULT, INTERFACE_abm, INTERFACE_abs {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_aci_DEFAULT [INTERFACE_abq_DEFAULT ] [INTERFACE_abm ] [INTERFACE_abs [INTERFACE_abq_DEFAULT ] [INTERFACE_abr ]]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_aci_DEFAULT [INTERFACE_abq_DEFAULT ] [INTERFACE_abm ] [INTERFACE_abs [INTERFACE_abq_DEFAULT ] [INTERFACE_abr ]]]"
  return-object v0
.end method


# }

