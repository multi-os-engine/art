
.class public abstract interface LINTERFACE_aae_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_aac;

.implements LINTERFACE_aad;


# public interface INTERFACE_aae_DEFAULT extends   INTERFACE_aac, INTERFACE_aad {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_aae_DEFAULT [INTERFACE_aac ] [INTERFACE_aad ]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_aae_DEFAULT [INTERFACE_aac ] [INTERFACE_aad ]]"
  return-object v0
.end method


# }

