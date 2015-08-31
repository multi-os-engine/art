
.class public abstract interface LINTERFACE_abg_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_abb;

.implements LINTERFACE_aba_DEFAULT;

.implements LINTERFACE_abd;


# public interface INTERFACE_abg_DEFAULT extends   INTERFACE_abb, INTERFACE_aba_DEFAULT, INTERFACE_abd {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_abg_DEFAULT [INTERFACE_abb ] [INTERFACE_aba_DEFAULT ] [INTERFACE_abd [INTERFACE_abb ] [INTERFACE_abc ]]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_abg_DEFAULT [INTERFACE_abb ] [INTERFACE_aba_DEFAULT ] [INTERFACE_abd [INTERFACE_abb ] [INTERFACE_abc ]]]"
  return-object v0
.end method


# }

