.class public LB27148248;

# Regression for dex2oatd crash during compilation of method which
# use throw with argument of non-reference type.

.super Ljava/lang/Object;

.method public static run()V
   .registers 1
   const v0, 0xbad
   throw v0
.end method

