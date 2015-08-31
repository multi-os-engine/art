
.class public abstract interface LINTERFACE_aau_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_aaq;

.implements LINTERFACE_aao;

.implements LINTERFACE_aar;


# public interface INTERFACE_aau_DEFAULT extends   INTERFACE_aaq, INTERFACE_aao, INTERFACE_aar {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_aau_DEFAULT [INTERFACE_aaq ] [INTERFACE_aao ] [INTERFACE_aar [INTERFACE_aap ] [INTERFACE_aaq ]]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_aau_DEFAULT [INTERFACE_aaq ] [INTERFACE_aao ] [INTERFACE_aar [INTERFACE_aap ] [INTERFACE_aaq ]]]"
  return-object v0
.end method


# }

