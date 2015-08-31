
.class public abstract interface LINTERFACE_aas_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_aao;

.implements LINTERFACE_aar;


# public interface INTERFACE_aas_DEFAULT extends   INTERFACE_aao, INTERFACE_aar {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_aas_DEFAULT [INTERFACE_aao ] [INTERFACE_aar [INTERFACE_aap ] [INTERFACE_aaq ]]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_aas_DEFAULT [INTERFACE_aao ] [INTERFACE_aar [INTERFACE_aap ] [INTERFACE_aaq ]]]"
  return-object v0
.end method


# }

