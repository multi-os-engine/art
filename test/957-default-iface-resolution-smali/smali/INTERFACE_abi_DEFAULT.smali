
.class public abstract interface LINTERFACE_abi_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_abc;

.implements LINTERFACE_aba_DEFAULT;

.implements LINTERFACE_abd;


# public interface INTERFACE_abi_DEFAULT extends   INTERFACE_abc, INTERFACE_aba_DEFAULT, INTERFACE_abd {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_abi_DEFAULT [INTERFACE_abc ] [INTERFACE_aba_DEFAULT ] [INTERFACE_abd [INTERFACE_abb ] [INTERFACE_abc ]]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_abi_DEFAULT [INTERFACE_abc ] [INTERFACE_aba_DEFAULT ] [INTERFACE_abd [INTERFACE_abb ] [INTERFACE_abc ]]]"
  return-object v0
.end method


# }

