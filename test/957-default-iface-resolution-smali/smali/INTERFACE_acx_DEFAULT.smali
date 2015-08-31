
.class public abstract interface LINTERFACE_acx_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_acu;

.implements LINTERFACE_acv;

.implements LINTERFACE_acw;


# public interface INTERFACE_acx_DEFAULT extends   INTERFACE_acu, INTERFACE_acv, INTERFACE_acw {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_acx_DEFAULT [INTERFACE_acu ] [INTERFACE_acv ] [INTERFACE_acw ]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_acx_DEFAULT [INTERFACE_acu ] [INTERFACE_acv ] [INTERFACE_acw ]]"
  return-object v0
.end method


# }

