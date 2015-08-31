
.class public abstract interface LINTERFACE_adl_DEFAULT;
.super Ljava/lang/Object;
 

# public interface INTERFACE_adl_DEFAULT   {

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_adl_DEFAULT ]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_adl_DEFAULT ]"
  return-object v0
.end method


# }

