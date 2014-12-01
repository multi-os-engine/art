public class ConstantFolding {

  /**
   * Tiny three-register program exercising int constant folding
   * on negation.
   */

  public static int $001_IntNegation() {
    int x, y;
    x = 42;
    y = -x;
    return y;
  }

  // CHECK:   BEGIN_METHOD ConstantFolding.$001_IntNegation
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [before]
  // CHECK:     [[Const42:[0-9]+]]:  IntConstant 42
  // CHECK:     [[Neg:[0-9]+]]:      Neg([[Const42]])
  // CHECK:                          Return([[Neg]])
  // CHECK:   END_GRAPH_DUMP constant_folding [before]
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [after]
  // CHECK:     [[Const42]]:         IntConstant 42
  // CHECK:     [[ConstN42:[0-9]+]]: IntConstant -42
  // CHECK:                          Return([[ConstN42]])
  // CHECK:   END_GRAPH_DUMP constant_folding [after]
  // CHECK:   END_METHOD ConstantFolding.$001_IntNegation


  /**
   * Tiny three-register program exercising int constant folding
   * on addition.
   */

  public static int $002_IntAddition1() {
    int a, b, c;
    a = 1;
    b = 2;
    c = a + b;
    return c;
  }

  // CHECK:   BEGIN_METHOD ConstantFolding.$002_IntAddition1
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [before]
  // CHECK:     [[Const1:[0-9]+]]:  IntConstant 1
  // CHECK:     [[Const2:[0-9]+]]:  IntConstant 2
  // CHECK:     [[Add:[0-9]+]]:     Add([[Const1]], [[Const2]])
  // CHECK:                         Return([[Add]])
  // CHECK:   END_GRAPH_DUMP constant_folding [before]
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [after]
  // CHECK:     [[Const1]]:         IntConstant 1
  // CHECK:     [[Const2]]:         IntConstant 2
  // CHECK:     [[Add:[0-9]+]]:     IntConstant 3
  // CHECK:                         Return([[Add]])
  // CHECK:   END_GRAPH_DUMP constant_folding [after]
  // CHECK:   END_METHOD ConstantFolding.$002_IntAddition1


 /**
  * Small three-register program exercising int constant folding
  * on addition.
  */

  public static int $003_IntAddition2() {
    int a, b, c;
    a = 1;
    b = 2;
    a += b;
    b = 5;
    c = 6;
    b += c;
    c = a + b;
    return c;
  }

  // CHECK:   BEGIN_METHOD ConstantFolding.$003_IntAddition2
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [before]
  // CHECK:     [[Const1:[0-9]+]]:  IntConstant 1
  // CHECK:     [[Const2:[0-9]+]]:  IntConstant 2
  // CHECK:     [[Const5:[0-9]+]]:  IntConstant 5
  // CHECK:     [[Const6:[0-9]+]]:  IntConstant 6
  // CHECK:     [[Add1:[0-9]+]]:    Add([[Const1]], [[Const2]])
  // CHECK:     [[Add2:[0-9]+]]:    Add([[Const5]], [[Const6]])
  // CHECK:     [[Add3:[0-9]+]]:    Add([[Add1]], [[Add2]])
  // CHECK:                         Return([[Add3]])
  // CHECK:   END_GRAPH_DUMP constant_folding [before]
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [after]
  // CHECK:     [[Const1]]:         IntConstant 1
  // CHECK:     [[Const2]]:         IntConstant 2
  // CHECK:     [[Const5]]:         IntConstant 5
  // CHECK:     [[Const6]]:         IntConstant 6
  // CHECK:                         IntConstant 3
  // CHECK:                         IntConstant 11
  // CHECK:     [[Const14:[0-9]+]]: IntConstant 14
  // CHECK:                         Return([[Const14]])
  // CHECK:   END_GRAPH_DUMP constant_folding [after]
  // CHECK:   END_METHOD ConstantFolding.$003_IntAddition2
  

  /**
   * Tiny three-register program exercising int constant folding
   * on subtraction.
   */

  public static int $004_IntSubtraction() {
    int a, b, c;
    a = 5;
    b = 2;
    c = a - b;
    return c;
  }
  
  // CHECK:   BEGIN_METHOD ConstantFolding.$004_IntSubtraction
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [before]
  // CHECK:     [[Const5:[0-9]+]]: IntConstant 5
  // CHECK:     [[Const2:[0-9]+]]: IntConstant 2
  // CHECK:     [[Sub:[0-9]+]]:    Sub([[Const5]], [[Const2]])
  // CHECK:                        Return([[Sub]])
  // CHECK:   END_GRAPH_DUMP constant_folding [before]
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [after]
  // CHECK:     [[Const5]]:        IntConstant 5
  // CHECK:     [[Const2]]:        IntConstant 2
  // CHECK:     [[Const3:[0-9]+]]: IntConstant 3
  // CHECK:                        Return([[Const3]])
  // CHECK:   END_GRAPH_DUMP constant_folding [after]
  // CHECK:   END_METHOD ConstantFolding.$004_IntSubtraction


  /**
   * Tiny three-register program exercising long constant folding
   * on addition.
   */

  public static long $005_LongAddition() {
    long a, b, c;
    a = 1L;
    b = 2L;
    c = a + b;
    return c;
  }
  
  // CHECK:   BEGIN_METHOD ConstantFolding.$005_LongAddition
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [before]
  // CHECK:     [[Const1:[0-9]+]]: LongConstant 1
  // CHECK:     [[Const2:[0-9]+]]: LongConstant 2
  // CHECK:     [[Add:[0-9]+]]:    Add([[Const1]], [[Const2]])
  // CHECK:                        Return([[Add]])
  // CHECK:   END_GRAPH_DUMP constant_folding [before]
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [after]
  // CHECK:     [[Const1]]:        LongConstant 1
  // CHECK:     [[Const2]]:        LongConstant 2
  // CHECK:     [[Const3:[0-9]+]]: LongConstant 3
  // CHECK:                        Return([[Const3]])
  // CHECK:   END_GRAPH_DUMP constant_folding [after]
  // CHECK:   END_METHOD ConstantFolding.$005_LongAddition


  /**
   * Tiny three-register program exercising long constant folding
   * on subtraction.
   */

  public static long $006_LongSubtraction() {
    long a, b, c;
    a = 5L;
    b = 2L;
    c = a - b;
    return c;
  }

  
  // CHECK:   BEGIN_METHOD ConstantFolding.$006_LongSubtraction
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [before]
  // CHECK:     [[Const5:[0-9]+]]: LongConstant 5
  // CHECK:     [[Const2:[0-9]+]]: LongConstant 2
  // CHECK:     [[Sub:[0-9]+]]:    Sub([[Const5]], [[Const2]])
  // CHECK:                        Return([[Sub]])
  // CHECK:   END_GRAPH_DUMP constant_folding [before]
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [after]
  // CHECK:     [[Const5]]:        LongConstant 5
  // CHECK:     [[Const2]]:        LongConstant 2
  // CHECK:     [[Const3:[0-9]+]]: LongConstant 3
  // CHECK:                        Return([[Const3]])
  // CHECK:   END_GRAPH_DUMP constant_folding [after]
  // CHECK:   END_METHOD ConstantFolding.$006_LongSubtraction


  /**
   * Three-register program with a constant (static) condition.
   */

  public static int $007_StaticCondition() {
    int a, b, c;
    a = 5;
    b = 2;
    if (a < b)
      c = a + b;
    else
      c = a - b;
    return c;
  }
  
  // CHECK:   BEGIN_METHOD ConstantFolding.$007_StaticCondition
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [before]
  // CHECK:     [[Const5:[0-9]+]]: IntConstant 5
  // CHECK:     [[Const2:[0-9]+]]: IntConstant 2
  // CHECK:     [[Cond:[0-9]+]]:   GreaterThanOrEqual([[Const5]], [[Const2]])
  // CHECK:                        If([[Cond]])
  // CHECK:   END_GRAPH_DUMP constant_folding [before]
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [after]
  // CHECK:     [[Const5]]:        IntConstant 5
  // CHECK:     [[Const2]]:        IntConstant 2
  // CHECK:     [[Const1:[0-9]+]]: IntConstant 1
  // CHECK:                        If([[Const1]])
  // CHECK:   END_GRAPH_DUMP constant_folding [after]
  // CHECK:   END_METHOD ConstantFolding.$007_StaticCondition


  /**
   * Four-variable program with jumps leading to the creation of many
   * blocks.
   *
   * The intent of this test is to ensure that all constant expressions
   * are actually evaluated at compile-time, thanks to the reverse
   * (forward) post-order traversal of the the dominator tree.
   */

  public static int $008_JumpsAndConditionals(boolean cond) {
    int a, b, c;
    a = 5;
    b = 2;
    if (cond)
      c = a + b;
    else
      c = a - b;
    return c;
  }
  
  // CHECK:   BEGIN_METHOD ConstantFolding.$008_JumpsAndConditionals
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [before]
  // CHECK:     [[Const5:[0-9]+]]: IntConstant 5
  // CHECK:     [[Const2:[0-9]+]]: IntConstant 2
  // CHECK:     [[Add:[0-9]+]]:    Add([[Const5]], [[Const2]])
  // CHECK:     [[Phi:[0-9]+]]:    Phi([[Add]], [[Sub:[0-9]+]])
  // CHECK:                        Return([[Phi]])
  // CHECK:     [[Sub]]:           Sub([[Const5]], [[Const2]])
  // CHECK:   END_GRAPH_DUMP constant_folding [before]
  // CHECK:   BEGIN_GRAPH_DUMP constant_folding [after]
  // CHECK:     [[Const5]]:        IntConstant 5
  // CHECK:     [[Const2]]:        IntConstant 2
  // CHECK:     [[Const7:[0-9]+]]: IntConstant 7
  // CHECK:     [[Phi]]:           Phi([[Const7]], [[Const3:[0-9]+]])
  // CHECK:                        Return([[Phi]])
  // CHECK:     [[Const3]]:        IntConstant 3
  // CHECK:   END_GRAPH_DUMP constant_folding [after]
  // CHECK:   END_METHOD ConstantFolding.$008_JumpsAndConditionals
}

