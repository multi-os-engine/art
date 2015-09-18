public class Main {

  public static int $noinline$intValue(int value) {
    if (doThrow) { throw new Error(); }
    return value;
  }

  public static long $noinline$longValue(long value) {
    if (doThrow) { throw new Error(); }
    return value;
  }

  public static float $noinline$floatValue(float value) {
    if (doThrow) { throw new Error(); }
    return value;
  }

  public static double $noinline$doubleValue(double value) {
    if (doThrow) { throw new Error(); }
    return value;
  }

  /// CHECK-START: int Main.testCase_ZEQ_I() register (after)
  /// CHECK:         Select

  public static int testCase_ZEQ_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (boolCond) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_IEQ_I() register (after)
  /// CHECK:         Select

  public static int testCase_IEQ_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (intValue1 == intValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_INE_I() register (after)
  /// CHECK:         Select

  public static int testCase_INE_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (intValue1 != intValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_IGT_I() register (after)
  /// CHECK:         Select

  public static int testCase_IGT_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (intValue1 > intValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_ILT_I() register (after)
  /// CHECK:         Select

  public static int testCase_ILT_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (intValue1 < intValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_IGE_I() register (after)
  /// CHECK:         Select

  public static int testCase_IGE_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (intValue1 >= intValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_ILE_I() register (after)
  /// CHECK:         Select

  public static int testCase_ILE_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (intValue1 <= intValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_JEQ_I() register (after)
  /// CHECK:         Select

  public static int testCase_JEQ_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (longValue1 == longValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_JNE_I() register (after)
  /// CHECK:         Select

  public static int testCase_JNE_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (longValue1 != longValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_JGT_I() register (after)
  /// CHECK:         Select

  public static int testCase_JGT_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (longValue1 > longValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_JLT_I() register (after)
  /// CHECK:         Select

  public static int testCase_JLT_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (longValue1 < longValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_JGE_I() register (after)
  /// CHECK:         Select

  public static int testCase_JGE_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (longValue1 >= longValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_JLE_I() register (after)
  /// CHECK:         Select

  public static int testCase_JLE_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (longValue1 <= longValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_FEQ_I() register (after)
  /// CHECK:         Select

  public static int testCase_FEQ_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (floatValue1 == floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_FNE_I() register (after)
  /// CHECK:         Select

  public static int testCase_FNE_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (floatValue1 != floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_FGT_I() register (after)
  /// CHECK:         Select

  public static int testCase_FGT_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (floatValue1 > floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_FLT_I() register (after)
  /// CHECK:         Select

  public static int testCase_FLT_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (floatValue1 < floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_FGE_I() register (after)
  /// CHECK:         Select

  public static int testCase_FGE_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (floatValue1 >= floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_FLE_I() register (after)
  /// CHECK:         Select

  public static int testCase_FLE_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (floatValue1 <= floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_DEQ_I() register (after)
  /// CHECK:         Select

  public static int testCase_DEQ_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (doubleValue1 == doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_DNE_I() register (after)
  /// CHECK:         Select

  public static int testCase_DNE_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (doubleValue1 != doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_DGT_I() register (after)
  /// CHECK:         Select

  public static int testCase_DGT_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (doubleValue1 > doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_DLT_I() register (after)
  /// CHECK:         Select

  public static int testCase_DLT_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (doubleValue1 < doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_DGE_I() register (after)
  /// CHECK:         Select

  public static int testCase_DGE_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (doubleValue1 >= doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: int Main.testCase_DLE_I() register (after)
  /// CHECK:         Select

  public static int testCase_DLE_I() {
    int val_true = $noinline$intValue(111);
    int val_false = $noinline$intValue(222);
    return (doubleValue1 <= doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_ZEQ_J() register (after)
  /// CHECK:         Select

  public static long testCase_ZEQ_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (boolCond) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_IEQ_J() register (after)
  /// CHECK:         Select

  public static long testCase_IEQ_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (intValue1 == intValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_INE_J() register (after)
  /// CHECK:         Select

  public static long testCase_INE_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (intValue1 != intValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_IGT_J() register (after)
  /// CHECK:         Select

  public static long testCase_IGT_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (intValue1 > intValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_ILT_J() register (after)
  /// CHECK:         Select

  public static long testCase_ILT_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (intValue1 < intValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_IGE_J() register (after)
  /// CHECK:         Select

  public static long testCase_IGE_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (intValue1 >= intValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_ILE_J() register (after)
  /// CHECK:         Select

  public static long testCase_ILE_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (intValue1 <= intValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_JEQ_J() register (after)
  /// CHECK:         Select

  public static long testCase_JEQ_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (longValue1 == longValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_JNE_J() register (after)
  /// CHECK:         Select

  public static long testCase_JNE_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (longValue1 != longValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_JGT_J() register (after)
  /// CHECK:         Select

  public static long testCase_JGT_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (longValue1 > longValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_JLT_J() register (after)
  /// CHECK:         Select

  public static long testCase_JLT_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (longValue1 < longValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_JGE_J() register (after)
  /// CHECK:         Select

  public static long testCase_JGE_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (longValue1 >= longValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_JLE_J() register (after)
  /// CHECK:         Select

  public static long testCase_JLE_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (longValue1 <= longValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_FEQ_J() register (after)
  /// CHECK:         Select

  public static long testCase_FEQ_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (floatValue1 == floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_FNE_J() register (after)
  /// CHECK:         Select

  public static long testCase_FNE_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (floatValue1 != floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_FGT_J() register (after)
  /// CHECK:         Select

  public static long testCase_FGT_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (floatValue1 > floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_FLT_J() register (after)
  /// CHECK:         Select

  public static long testCase_FLT_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (floatValue1 < floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_FGE_J() register (after)
  /// CHECK:         Select

  public static long testCase_FGE_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (floatValue1 >= floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_FLE_J() register (after)
  /// CHECK:         Select

  public static long testCase_FLE_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (floatValue1 <= floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_DEQ_J() register (after)
  /// CHECK:         Select

  public static long testCase_DEQ_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (doubleValue1 == doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_DNE_J() register (after)
  /// CHECK:         Select

  public static long testCase_DNE_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (doubleValue1 != doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_DGT_J() register (after)
  /// CHECK:         Select

  public static long testCase_DGT_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (doubleValue1 > doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_DLT_J() register (after)
  /// CHECK:         Select

  public static long testCase_DLT_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (doubleValue1 < doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_DGE_J() register (after)
  /// CHECK:         Select

  public static long testCase_DGE_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (doubleValue1 >= doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: long Main.testCase_DLE_J() register (after)
  /// CHECK:         Select

  public static long testCase_DLE_J() {
    long val_true = $noinline$longValue(111);
    long val_false = $noinline$longValue(222);
    return (doubleValue1 <= doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_ZEQ_F() register (after)
  /// CHECK:         Select

  public static float testCase_ZEQ_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (boolCond) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_IEQ_F() register (after)
  /// CHECK:         Select

  public static float testCase_IEQ_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (intValue1 == intValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_INE_F() register (after)
  /// CHECK:         Select

  public static float testCase_INE_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (intValue1 != intValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_IGT_F() register (after)
  /// CHECK:         Select

  public static float testCase_IGT_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (intValue1 > intValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_ILT_F() register (after)
  /// CHECK:         Select

  public static float testCase_ILT_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (intValue1 < intValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_IGE_F() register (after)
  /// CHECK:         Select

  public static float testCase_IGE_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (intValue1 >= intValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_ILE_F() register (after)
  /// CHECK:         Select

  public static float testCase_ILE_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (intValue1 <= intValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_JEQ_F() register (after)
  /// CHECK:         Select

  public static float testCase_JEQ_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (longValue1 == longValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_JNE_F() register (after)
  /// CHECK:         Select

  public static float testCase_JNE_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (longValue1 != longValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_JGT_F() register (after)
  /// CHECK:         Select

  public static float testCase_JGT_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (longValue1 > longValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_JLT_F() register (after)
  /// CHECK:         Select

  public static float testCase_JLT_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (longValue1 < longValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_JGE_F() register (after)
  /// CHECK:         Select

  public static float testCase_JGE_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (longValue1 >= longValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_JLE_F() register (after)
  /// CHECK:         Select

  public static float testCase_JLE_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (longValue1 <= longValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_FEQ_F() register (after)
  /// CHECK:         Select

  public static float testCase_FEQ_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (floatValue1 == floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_FNE_F() register (after)
  /// CHECK:         Select

  public static float testCase_FNE_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (floatValue1 != floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_FGT_F() register (after)
  /// CHECK:         Select

  public static float testCase_FGT_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (floatValue1 > floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_FLT_F() register (after)
  /// CHECK:         Select

  public static float testCase_FLT_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (floatValue1 < floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_FGE_F() register (after)
  /// CHECK:         Select

  public static float testCase_FGE_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (floatValue1 >= floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_FLE_F() register (after)
  /// CHECK:         Select

  public static float testCase_FLE_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (floatValue1 <= floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_DEQ_F() register (after)
  /// CHECK:         Select

  public static float testCase_DEQ_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (doubleValue1 == doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_DNE_F() register (after)
  /// CHECK:         Select

  public static float testCase_DNE_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (doubleValue1 != doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_DGT_F() register (after)
  /// CHECK:         Select

  public static float testCase_DGT_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (doubleValue1 > doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_DLT_F() register (after)
  /// CHECK:         Select

  public static float testCase_DLT_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (doubleValue1 < doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_DGE_F() register (after)
  /// CHECK:         Select

  public static float testCase_DGE_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (doubleValue1 >= doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: float Main.testCase_DLE_F() register (after)
  /// CHECK:         Select

  public static float testCase_DLE_F() {
    float val_true = $noinline$floatValue(111);
    float val_false = $noinline$floatValue(222);
    return (doubleValue1 <= doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_ZEQ_D() register (after)
  /// CHECK:         Select

  public static double testCase_ZEQ_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (boolCond) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_IEQ_D() register (after)
  /// CHECK:         Select

  public static double testCase_IEQ_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (intValue1 == intValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_INE_D() register (after)
  /// CHECK:         Select

  public static double testCase_INE_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (intValue1 != intValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_IGT_D() register (after)
  /// CHECK:         Select

  public static double testCase_IGT_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (intValue1 > intValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_ILT_D() register (after)
  /// CHECK:         Select

  public static double testCase_ILT_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (intValue1 < intValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_IGE_D() register (after)
  /// CHECK:         Select

  public static double testCase_IGE_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (intValue1 >= intValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_ILE_D() register (after)
  /// CHECK:         Select

  public static double testCase_ILE_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (intValue1 <= intValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_JEQ_D() register (after)
  /// CHECK:         Select

  public static double testCase_JEQ_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (longValue1 == longValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_JNE_D() register (after)
  /// CHECK:         Select

  public static double testCase_JNE_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (longValue1 != longValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_JGT_D() register (after)
  /// CHECK:         Select

  public static double testCase_JGT_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (longValue1 > longValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_JLT_D() register (after)
  /// CHECK:         Select

  public static double testCase_JLT_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (longValue1 < longValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_JGE_D() register (after)
  /// CHECK:         Select

  public static double testCase_JGE_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (longValue1 >= longValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_JLE_D() register (after)
  /// CHECK:         Select

  public static double testCase_JLE_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (longValue1 <= longValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_FEQ_D() register (after)
  /// CHECK:         Select

  public static double testCase_FEQ_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (floatValue1 == floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_FNE_D() register (after)
  /// CHECK:         Select

  public static double testCase_FNE_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (floatValue1 != floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_FGT_D() register (after)
  /// CHECK:         Select

  public static double testCase_FGT_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (floatValue1 > floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_FLT_D() register (after)
  /// CHECK:         Select

  public static double testCase_FLT_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (floatValue1 < floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_FGE_D() register (after)
  /// CHECK:         Select

  public static double testCase_FGE_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (floatValue1 >= floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_FLE_D() register (after)
  /// CHECK:         Select

  public static double testCase_FLE_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (floatValue1 <= floatValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_DEQ_D() register (after)
  /// CHECK:         Select

  public static double testCase_DEQ_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (doubleValue1 == doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_DNE_D() register (after)
  /// CHECK:         Select

  public static double testCase_DNE_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (doubleValue1 != doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_DGT_D() register (after)
  /// CHECK:         Select

  public static double testCase_DGT_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (doubleValue1 > doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_DLT_D() register (after)
  /// CHECK:         Select

  public static double testCase_DLT_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (doubleValue1 < doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_DGE_D() register (after)
  /// CHECK:         Select

  public static double testCase_DGE_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (doubleValue1 >= doubleValue2) ? val_true : val_false;
  }

  /// CHECK-START: double Main.testCase_DLE_D() register (after)
  /// CHECK:         Select

  public static double testCase_DLE_D() {
    double val_true = $noinline$doubleValue(111);
    double val_false = $noinline$doubleValue(222);
    return (doubleValue1 <= doubleValue2) ? val_true : val_false;
  }

  public static void main(String[] args) {
    int val;

    boolCond = true;
    val = (int) testCase_ZEQ_I();
    if (val != 111) {
      throw new Error("True testCase_ZEQ_I returned: " + val);
    }
    boolCond = false;
    val = (int) testCase_ZEQ_I();
    if (val != 222) {
      throw new Error("False testCase_ZEQ_I returned: " + val);
    }

    intValue1 = 42; intValue2 = 42;
    val = (int) testCase_IEQ_I();
    if (val != 111) {
      throw new Error("True testCase_IEQ_I returned: " + val);
    }
    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_IEQ_I();
    if (val != 222) {
      throw new Error("False testCase_IEQ_I returned: " + val);
    }

    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_INE_I();
    if (val != 111) {
      throw new Error("True testCase_INE_I returned: " + val);
    }
    intValue1 = 42; intValue2 = 42;
    val = (int) testCase_INE_I();
    if (val != 222) {
      throw new Error("False testCase_INE_I returned: " + val);
    }

    intValue1 = 42; intValue2 = 41;
    val = (int) testCase_IGT_I();
    if (val != 111) {
      throw new Error("True testCase_IGT_I returned: " + val);
    }
    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_IGT_I();
    if (val != 222) {
      throw new Error("False testCase_IGT_I returned: " + val);
    }

    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_ILT_I();
    if (val != 111) {
      throw new Error("True testCase_ILT_I returned: " + val);
    }
    intValue1 = 43; intValue2 = 42;
    val = (int) testCase_ILT_I();
    if (val != 222) {
      throw new Error("False testCase_ILT_I returned: " + val);
    }

    intValue1 = 42; intValue2 = 41;
    val = (int) testCase_IGE_I();
    if (val != 111) {
      throw new Error("True testCase_IGE_I returned: " + val);
    }
    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_IGE_I();
    if (val != 222) {
      throw new Error("False testCase_IGE_I returned: " + val);
    }

    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_ILE_I();
    if (val != 111) {
      throw new Error("True testCase_ILE_I returned: " + val);
    }
    intValue1 = 43; intValue2 = 42;
    val = (int) testCase_ILE_I();
    if (val != 222) {
      throw new Error("False testCase_ILE_I returned: " + val);
    }

    longValue1 = 42; longValue2 = 42;
    val = (int) testCase_JEQ_I();
    if (val != 111) {
      throw new Error("True testCase_JEQ_I returned: " + val);
    }
    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JEQ_I();
    if (val != 222) {
      throw new Error("False testCase_JEQ_I returned: " + val);
    }

    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JNE_I();
    if (val != 111) {
      throw new Error("True testCase_JNE_I returned: " + val);
    }
    longValue1 = 42; longValue2 = 42;
    val = (int) testCase_JNE_I();
    if (val != 222) {
      throw new Error("False testCase_JNE_I returned: " + val);
    }

    longValue1 = 42; longValue2 = 41;
    val = (int) testCase_JGT_I();
    if (val != 111) {
      throw new Error("True testCase_JGT_I returned: " + val);
    }
    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JGT_I();
    if (val != 222) {
      throw new Error("False testCase_JGT_I returned: " + val);
    }

    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JLT_I();
    if (val != 111) {
      throw new Error("True testCase_JLT_I returned: " + val);
    }
    longValue1 = 43; longValue2 = 42;
    val = (int) testCase_JLT_I();
    if (val != 222) {
      throw new Error("False testCase_JLT_I returned: " + val);
    }

    longValue1 = 42; longValue2 = 41;
    val = (int) testCase_JGE_I();
    if (val != 111) {
      throw new Error("True testCase_JGE_I returned: " + val);
    }
    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JGE_I();
    if (val != 222) {
      throw new Error("False testCase_JGE_I returned: " + val);
    }

    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JLE_I();
    if (val != 111) {
      throw new Error("True testCase_JLE_I returned: " + val);
    }
    longValue1 = 43; longValue2 = 42;
    val = (int) testCase_JLE_I();
    if (val != 222) {
      throw new Error("False testCase_JLE_I returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 42;
    val = (int) testCase_FEQ_I();
    if (val != 111) {
      throw new Error("True testCase_FEQ_I returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FEQ_I();
    if (val != 222) {
      throw new Error("False testCase_FEQ_I returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FNE_I();
    if (val != 111) {
      throw new Error("True testCase_FNE_I returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 42;
    val = (int) testCase_FNE_I();
    if (val != 222) {
      throw new Error("False testCase_FNE_I returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 41;
    val = (int) testCase_FGT_I();
    if (val != 111) {
      throw new Error("True testCase_FGT_I returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FGT_I();
    if (val != 222) {
      throw new Error("False testCase_FGT_I returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FLT_I();
    if (val != 111) {
      throw new Error("True testCase_FLT_I returned: " + val);
    }
    floatValue1 = 43; floatValue2 = 42;
    val = (int) testCase_FLT_I();
    if (val != 222) {
      throw new Error("False testCase_FLT_I returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 41;
    val = (int) testCase_FGE_I();
    if (val != 111) {
      throw new Error("True testCase_FGE_I returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FGE_I();
    if (val != 222) {
      throw new Error("False testCase_FGE_I returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FLE_I();
    if (val != 111) {
      throw new Error("True testCase_FLE_I returned: " + val);
    }
    floatValue1 = 43; floatValue2 = 42;
    val = (int) testCase_FLE_I();
    if (val != 222) {
      throw new Error("False testCase_FLE_I returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 42;
    val = (int) testCase_DEQ_I();
    if (val != 111) {
      throw new Error("True testCase_DEQ_I returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DEQ_I();
    if (val != 222) {
      throw new Error("False testCase_DEQ_I returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DNE_I();
    if (val != 111) {
      throw new Error("True testCase_DNE_I returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 42;
    val = (int) testCase_DNE_I();
    if (val != 222) {
      throw new Error("False testCase_DNE_I returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 41;
    val = (int) testCase_DGT_I();
    if (val != 111) {
      throw new Error("True testCase_DGT_I returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DGT_I();
    if (val != 222) {
      throw new Error("False testCase_DGT_I returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DLT_I();
    if (val != 111) {
      throw new Error("True testCase_DLT_I returned: " + val);
    }
    doubleValue1 = 43; doubleValue2 = 42;
    val = (int) testCase_DLT_I();
    if (val != 222) {
      throw new Error("False testCase_DLT_I returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 41;
    val = (int) testCase_DGE_I();
    if (val != 111) {
      throw new Error("True testCase_DGE_I returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DGE_I();
    if (val != 222) {
      throw new Error("False testCase_DGE_I returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DLE_I();
    if (val != 111) {
      throw new Error("True testCase_DLE_I returned: " + val);
    }
    doubleValue1 = 43; doubleValue2 = 42;
    val = (int) testCase_DLE_I();
    if (val != 222) {
      throw new Error("False testCase_DLE_I returned: " + val);
    }

    boolCond = true;
    val = (int) testCase_ZEQ_J();
    if (val != 111) {
      throw new Error("True testCase_ZEQ_J returned: " + val);
    }
    boolCond = false;
    val = (int) testCase_ZEQ_J();
    if (val != 222) {
      throw new Error("False testCase_ZEQ_J returned: " + val);
    }

    intValue1 = 42; intValue2 = 42;
    val = (int) testCase_IEQ_J();
    if (val != 111) {
      throw new Error("True testCase_IEQ_J returned: " + val);
    }
    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_IEQ_J();
    if (val != 222) {
      throw new Error("False testCase_IEQ_J returned: " + val);
    }

    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_INE_J();
    if (val != 111) {
      throw new Error("True testCase_INE_J returned: " + val);
    }
    intValue1 = 42; intValue2 = 42;
    val = (int) testCase_INE_J();
    if (val != 222) {
      throw new Error("False testCase_INE_J returned: " + val);
    }

    intValue1 = 42; intValue2 = 41;
    val = (int) testCase_IGT_J();
    if (val != 111) {
      throw new Error("True testCase_IGT_J returned: " + val);
    }
    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_IGT_J();
    if (val != 222) {
      throw new Error("False testCase_IGT_J returned: " + val);
    }

    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_ILT_J();
    if (val != 111) {
      throw new Error("True testCase_ILT_J returned: " + val);
    }
    intValue1 = 43; intValue2 = 42;
    val = (int) testCase_ILT_J();
    if (val != 222) {
      throw new Error("False testCase_ILT_J returned: " + val);
    }

    intValue1 = 42; intValue2 = 41;
    val = (int) testCase_IGE_J();
    if (val != 111) {
      throw new Error("True testCase_IGE_J returned: " + val);
    }
    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_IGE_J();
    if (val != 222) {
      throw new Error("False testCase_IGE_J returned: " + val);
    }

    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_ILE_J();
    if (val != 111) {
      throw new Error("True testCase_ILE_J returned: " + val);
    }
    intValue1 = 43; intValue2 = 42;
    val = (int) testCase_ILE_J();
    if (val != 222) {
      throw new Error("False testCase_ILE_J returned: " + val);
    }

    longValue1 = 42; longValue2 = 42;
    val = (int) testCase_JEQ_J();
    if (val != 111) {
      throw new Error("True testCase_JEQ_J returned: " + val);
    }
    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JEQ_J();
    if (val != 222) {
      throw new Error("False testCase_JEQ_J returned: " + val);
    }

    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JNE_J();
    if (val != 111) {
      throw new Error("True testCase_JNE_J returned: " + val);
    }
    longValue1 = 42; longValue2 = 42;
    val = (int) testCase_JNE_J();
    if (val != 222) {
      throw new Error("False testCase_JNE_J returned: " + val);
    }

    longValue1 = 42; longValue2 = 41;
    val = (int) testCase_JGT_J();
    if (val != 111) {
      throw new Error("True testCase_JGT_J returned: " + val);
    }
    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JGT_J();
    if (val != 222) {
      throw new Error("False testCase_JGT_J returned: " + val);
    }

    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JLT_J();
    if (val != 111) {
      throw new Error("True testCase_JLT_J returned: " + val);
    }
    longValue1 = 43; longValue2 = 42;
    val = (int) testCase_JLT_J();
    if (val != 222) {
      throw new Error("False testCase_JLT_J returned: " + val);
    }

    longValue1 = 42; longValue2 = 41;
    val = (int) testCase_JGE_J();
    if (val != 111) {
      throw new Error("True testCase_JGE_J returned: " + val);
    }
    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JGE_J();
    if (val != 222) {
      throw new Error("False testCase_JGE_J returned: " + val);
    }

    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JLE_J();
    if (val != 111) {
      throw new Error("True testCase_JLE_J returned: " + val);
    }
    longValue1 = 43; longValue2 = 42;
    val = (int) testCase_JLE_J();
    if (val != 222) {
      throw new Error("False testCase_JLE_J returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 42;
    val = (int) testCase_FEQ_J();
    if (val != 111) {
      throw new Error("True testCase_FEQ_J returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FEQ_J();
    if (val != 222) {
      throw new Error("False testCase_FEQ_J returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FNE_J();
    if (val != 111) {
      throw new Error("True testCase_FNE_J returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 42;
    val = (int) testCase_FNE_J();
    if (val != 222) {
      throw new Error("False testCase_FNE_J returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 41;
    val = (int) testCase_FGT_J();
    if (val != 111) {
      throw new Error("True testCase_FGT_J returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FGT_J();
    if (val != 222) {
      throw new Error("False testCase_FGT_J returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FLT_J();
    if (val != 111) {
      throw new Error("True testCase_FLT_J returned: " + val);
    }
    floatValue1 = 43; floatValue2 = 42;
    val = (int) testCase_FLT_J();
    if (val != 222) {
      throw new Error("False testCase_FLT_J returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 41;
    val = (int) testCase_FGE_J();
    if (val != 111) {
      throw new Error("True testCase_FGE_J returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FGE_J();
    if (val != 222) {
      throw new Error("False testCase_FGE_J returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FLE_J();
    if (val != 111) {
      throw new Error("True testCase_FLE_J returned: " + val);
    }
    floatValue1 = 43; floatValue2 = 42;
    val = (int) testCase_FLE_J();
    if (val != 222) {
      throw new Error("False testCase_FLE_J returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 42;
    val = (int) testCase_DEQ_J();
    if (val != 111) {
      throw new Error("True testCase_DEQ_J returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DEQ_J();
    if (val != 222) {
      throw new Error("False testCase_DEQ_J returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DNE_J();
    if (val != 111) {
      throw new Error("True testCase_DNE_J returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 42;
    val = (int) testCase_DNE_J();
    if (val != 222) {
      throw new Error("False testCase_DNE_J returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 41;
    val = (int) testCase_DGT_J();
    if (val != 111) {
      throw new Error("True testCase_DGT_J returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DGT_J();
    if (val != 222) {
      throw new Error("False testCase_DGT_J returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DLT_J();
    if (val != 111) {
      throw new Error("True testCase_DLT_J returned: " + val);
    }
    doubleValue1 = 43; doubleValue2 = 42;
    val = (int) testCase_DLT_J();
    if (val != 222) {
      throw new Error("False testCase_DLT_J returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 41;
    val = (int) testCase_DGE_J();
    if (val != 111) {
      throw new Error("True testCase_DGE_J returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DGE_J();
    if (val != 222) {
      throw new Error("False testCase_DGE_J returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DLE_J();
    if (val != 111) {
      throw new Error("True testCase_DLE_J returned: " + val);
    }
    doubleValue1 = 43; doubleValue2 = 42;
    val = (int) testCase_DLE_J();
    if (val != 222) {
      throw new Error("False testCase_DLE_J returned: " + val);
    }

    boolCond = true;
    val = (int) testCase_ZEQ_F();
    if (val != 111) {
      throw new Error("True testCase_ZEQ_F returned: " + val);
    }
    boolCond = false;
    val = (int) testCase_ZEQ_F();
    if (val != 222) {
      throw new Error("False testCase_ZEQ_F returned: " + val);
    }

    intValue1 = 42; intValue2 = 42;
    val = (int) testCase_IEQ_F();
    if (val != 111) {
      throw new Error("True testCase_IEQ_F returned: " + val);
    }
    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_IEQ_F();
    if (val != 222) {
      throw new Error("False testCase_IEQ_F returned: " + val);
    }

    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_INE_F();
    if (val != 111) {
      throw new Error("True testCase_INE_F returned: " + val);
    }
    intValue1 = 42; intValue2 = 42;
    val = (int) testCase_INE_F();
    if (val != 222) {
      throw new Error("False testCase_INE_F returned: " + val);
    }

    intValue1 = 42; intValue2 = 41;
    val = (int) testCase_IGT_F();
    if (val != 111) {
      throw new Error("True testCase_IGT_F returned: " + val);
    }
    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_IGT_F();
    if (val != 222) {
      throw new Error("False testCase_IGT_F returned: " + val);
    }

    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_ILT_F();
    if (val != 111) {
      throw new Error("True testCase_ILT_F returned: " + val);
    }
    intValue1 = 43; intValue2 = 42;
    val = (int) testCase_ILT_F();
    if (val != 222) {
      throw new Error("False testCase_ILT_F returned: " + val);
    }

    intValue1 = 42; intValue2 = 41;
    val = (int) testCase_IGE_F();
    if (val != 111) {
      throw new Error("True testCase_IGE_F returned: " + val);
    }
    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_IGE_F();
    if (val != 222) {
      throw new Error("False testCase_IGE_F returned: " + val);
    }

    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_ILE_F();
    if (val != 111) {
      throw new Error("True testCase_ILE_F returned: " + val);
    }
    intValue1 = 43; intValue2 = 42;
    val = (int) testCase_ILE_F();
    if (val != 222) {
      throw new Error("False testCase_ILE_F returned: " + val);
    }

    longValue1 = 42; longValue2 = 42;
    val = (int) testCase_JEQ_F();
    if (val != 111) {
      throw new Error("True testCase_JEQ_F returned: " + val);
    }
    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JEQ_F();
    if (val != 222) {
      throw new Error("False testCase_JEQ_F returned: " + val);
    }

    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JNE_F();
    if (val != 111) {
      throw new Error("True testCase_JNE_F returned: " + val);
    }
    longValue1 = 42; longValue2 = 42;
    val = (int) testCase_JNE_F();
    if (val != 222) {
      throw new Error("False testCase_JNE_F returned: " + val);
    }

    longValue1 = 42; longValue2 = 41;
    val = (int) testCase_JGT_F();
    if (val != 111) {
      throw new Error("True testCase_JGT_F returned: " + val);
    }
    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JGT_F();
    if (val != 222) {
      throw new Error("False testCase_JGT_F returned: " + val);
    }

    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JLT_F();
    if (val != 111) {
      throw new Error("True testCase_JLT_F returned: " + val);
    }
    longValue1 = 43; longValue2 = 42;
    val = (int) testCase_JLT_F();
    if (val != 222) {
      throw new Error("False testCase_JLT_F returned: " + val);
    }

    longValue1 = 42; longValue2 = 41;
    val = (int) testCase_JGE_F();
    if (val != 111) {
      throw new Error("True testCase_JGE_F returned: " + val);
    }
    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JGE_F();
    if (val != 222) {
      throw new Error("False testCase_JGE_F returned: " + val);
    }

    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JLE_F();
    if (val != 111) {
      throw new Error("True testCase_JLE_F returned: " + val);
    }
    longValue1 = 43; longValue2 = 42;
    val = (int) testCase_JLE_F();
    if (val != 222) {
      throw new Error("False testCase_JLE_F returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 42;
    val = (int) testCase_FEQ_F();
    if (val != 111) {
      throw new Error("True testCase_FEQ_F returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FEQ_F();
    if (val != 222) {
      throw new Error("False testCase_FEQ_F returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FNE_F();
    if (val != 111) {
      throw new Error("True testCase_FNE_F returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 42;
    val = (int) testCase_FNE_F();
    if (val != 222) {
      throw new Error("False testCase_FNE_F returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 41;
    val = (int) testCase_FGT_F();
    if (val != 111) {
      throw new Error("True testCase_FGT_F returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FGT_F();
    if (val != 222) {
      throw new Error("False testCase_FGT_F returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FLT_F();
    if (val != 111) {
      throw new Error("True testCase_FLT_F returned: " + val);
    }
    floatValue1 = 43; floatValue2 = 42;
    val = (int) testCase_FLT_F();
    if (val != 222) {
      throw new Error("False testCase_FLT_F returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 41;
    val = (int) testCase_FGE_F();
    if (val != 111) {
      throw new Error("True testCase_FGE_F returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FGE_F();
    if (val != 222) {
      throw new Error("False testCase_FGE_F returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FLE_F();
    if (val != 111) {
      throw new Error("True testCase_FLE_F returned: " + val);
    }
    floatValue1 = 43; floatValue2 = 42;
    val = (int) testCase_FLE_F();
    if (val != 222) {
      throw new Error("False testCase_FLE_F returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 42;
    val = (int) testCase_DEQ_F();
    if (val != 111) {
      throw new Error("True testCase_DEQ_F returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DEQ_F();
    if (val != 222) {
      throw new Error("False testCase_DEQ_F returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DNE_F();
    if (val != 111) {
      throw new Error("True testCase_DNE_F returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 42;
    val = (int) testCase_DNE_F();
    if (val != 222) {
      throw new Error("False testCase_DNE_F returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 41;
    val = (int) testCase_DGT_F();
    if (val != 111) {
      throw new Error("True testCase_DGT_F returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DGT_F();
    if (val != 222) {
      throw new Error("False testCase_DGT_F returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DLT_F();
    if (val != 111) {
      throw new Error("True testCase_DLT_F returned: " + val);
    }
    doubleValue1 = 43; doubleValue2 = 42;
    val = (int) testCase_DLT_F();
    if (val != 222) {
      throw new Error("False testCase_DLT_F returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 41;
    val = (int) testCase_DGE_F();
    if (val != 111) {
      throw new Error("True testCase_DGE_F returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DGE_F();
    if (val != 222) {
      throw new Error("False testCase_DGE_F returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DLE_F();
    if (val != 111) {
      throw new Error("True testCase_DLE_F returned: " + val);
    }
    doubleValue1 = 43; doubleValue2 = 42;
    val = (int) testCase_DLE_F();
    if (val != 222) {
      throw new Error("False testCase_DLE_F returned: " + val);
    }

    boolCond = true;
    val = (int) testCase_ZEQ_D();
    if (val != 111) {
      throw new Error("True testCase_ZEQ_D returned: " + val);
    }
    boolCond = false;
    val = (int) testCase_ZEQ_D();
    if (val != 222) {
      throw new Error("False testCase_ZEQ_D returned: " + val);
    }

    intValue1 = 42; intValue2 = 42;
    val = (int) testCase_IEQ_D();
    if (val != 111) {
      throw new Error("True testCase_IEQ_D returned: " + val);
    }
    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_IEQ_D();
    if (val != 222) {
      throw new Error("False testCase_IEQ_D returned: " + val);
    }

    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_INE_D();
    if (val != 111) {
      throw new Error("True testCase_INE_D returned: " + val);
    }
    intValue1 = 42; intValue2 = 42;
    val = (int) testCase_INE_D();
    if (val != 222) {
      throw new Error("False testCase_INE_D returned: " + val);
    }

    intValue1 = 42; intValue2 = 41;
    val = (int) testCase_IGT_D();
    if (val != 111) {
      throw new Error("True testCase_IGT_D returned: " + val);
    }
    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_IGT_D();
    if (val != 222) {
      throw new Error("False testCase_IGT_D returned: " + val);
    }

    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_ILT_D();
    if (val != 111) {
      throw new Error("True testCase_ILT_D returned: " + val);
    }
    intValue1 = 43; intValue2 = 42;
    val = (int) testCase_ILT_D();
    if (val != 222) {
      throw new Error("False testCase_ILT_D returned: " + val);
    }

    intValue1 = 42; intValue2 = 41;
    val = (int) testCase_IGE_D();
    if (val != 111) {
      throw new Error("True testCase_IGE_D returned: " + val);
    }
    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_IGE_D();
    if (val != 222) {
      throw new Error("False testCase_IGE_D returned: " + val);
    }

    intValue1 = 42; intValue2 = 43;
    val = (int) testCase_ILE_D();
    if (val != 111) {
      throw new Error("True testCase_ILE_D returned: " + val);
    }
    intValue1 = 43; intValue2 = 42;
    val = (int) testCase_ILE_D();
    if (val != 222) {
      throw new Error("False testCase_ILE_D returned: " + val);
    }

    longValue1 = 42; longValue2 = 42;
    val = (int) testCase_JEQ_D();
    if (val != 111) {
      throw new Error("True testCase_JEQ_D returned: " + val);
    }
    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JEQ_D();
    if (val != 222) {
      throw new Error("False testCase_JEQ_D returned: " + val);
    }

    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JNE_D();
    if (val != 111) {
      throw new Error("True testCase_JNE_D returned: " + val);
    }
    longValue1 = 42; longValue2 = 42;
    val = (int) testCase_JNE_D();
    if (val != 222) {
      throw new Error("False testCase_JNE_D returned: " + val);
    }

    longValue1 = 42; longValue2 = 41;
    val = (int) testCase_JGT_D();
    if (val != 111) {
      throw new Error("True testCase_JGT_D returned: " + val);
    }
    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JGT_D();
    if (val != 222) {
      throw new Error("False testCase_JGT_D returned: " + val);
    }

    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JLT_D();
    if (val != 111) {
      throw new Error("True testCase_JLT_D returned: " + val);
    }
    longValue1 = 43; longValue2 = 42;
    val = (int) testCase_JLT_D();
    if (val != 222) {
      throw new Error("False testCase_JLT_D returned: " + val);
    }

    longValue1 = 42; longValue2 = 41;
    val = (int) testCase_JGE_D();
    if (val != 111) {
      throw new Error("True testCase_JGE_D returned: " + val);
    }
    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JGE_D();
    if (val != 222) {
      throw new Error("False testCase_JGE_D returned: " + val);
    }

    longValue1 = 42; longValue2 = 43;
    val = (int) testCase_JLE_D();
    if (val != 111) {
      throw new Error("True testCase_JLE_D returned: " + val);
    }
    longValue1 = 43; longValue2 = 42;
    val = (int) testCase_JLE_D();
    if (val != 222) {
      throw new Error("False testCase_JLE_D returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 42;
    val = (int) testCase_FEQ_D();
    if (val != 111) {
      throw new Error("True testCase_FEQ_D returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FEQ_D();
    if (val != 222) {
      throw new Error("False testCase_FEQ_D returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FNE_D();
    if (val != 111) {
      throw new Error("True testCase_FNE_D returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 42;
    val = (int) testCase_FNE_D();
    if (val != 222) {
      throw new Error("False testCase_FNE_D returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 41;
    val = (int) testCase_FGT_D();
    if (val != 111) {
      throw new Error("True testCase_FGT_D returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FGT_D();
    if (val != 222) {
      throw new Error("False testCase_FGT_D returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FLT_D();
    if (val != 111) {
      throw new Error("True testCase_FLT_D returned: " + val);
    }
    floatValue1 = 43; floatValue2 = 42;
    val = (int) testCase_FLT_D();
    if (val != 222) {
      throw new Error("False testCase_FLT_D returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 41;
    val = (int) testCase_FGE_D();
    if (val != 111) {
      throw new Error("True testCase_FGE_D returned: " + val);
    }
    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FGE_D();
    if (val != 222) {
      throw new Error("False testCase_FGE_D returned: " + val);
    }

    floatValue1 = 42; floatValue2 = 43;
    val = (int) testCase_FLE_D();
    if (val != 111) {
      throw new Error("True testCase_FLE_D returned: " + val);
    }
    floatValue1 = 43; floatValue2 = 42;
    val = (int) testCase_FLE_D();
    if (val != 222) {
      throw new Error("False testCase_FLE_D returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 42;
    val = (int) testCase_DEQ_D();
    if (val != 111) {
      throw new Error("True testCase_DEQ_D returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DEQ_D();
    if (val != 222) {
      throw new Error("False testCase_DEQ_D returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DNE_D();
    if (val != 111) {
      throw new Error("True testCase_DNE_D returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 42;
    val = (int) testCase_DNE_D();
    if (val != 222) {
      throw new Error("False testCase_DNE_D returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 41;
    val = (int) testCase_DGT_D();
    if (val != 111) {
      throw new Error("True testCase_DGT_D returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DGT_D();
    if (val != 222) {
      throw new Error("False testCase_DGT_D returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DLT_D();
    if (val != 111) {
      throw new Error("True testCase_DLT_D returned: " + val);
    }
    doubleValue1 = 43; doubleValue2 = 42;
    val = (int) testCase_DLT_D();
    if (val != 222) {
      throw new Error("False testCase_DLT_D returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 41;
    val = (int) testCase_DGE_D();
    if (val != 111) {
      throw new Error("True testCase_DGE_D returned: " + val);
    }
    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DGE_D();
    if (val != 222) {
      throw new Error("False testCase_DGE_D returned: " + val);
    }

    doubleValue1 = 42; doubleValue2 = 43;
    val = (int) testCase_DLE_D();
    if (val != 111) {
      throw new Error("True testCase_DLE_D returned: " + val);
    }
    doubleValue1 = 43; doubleValue2 = 42;
    val = (int) testCase_DLE_D();
    if (val != 222) {
      throw new Error("False testCase_DLE_D returned: " + val);
    }
  }

  public static boolean boolCond;

  public static int intValue1, intValue2;
  public static long longValue1, longValue2;
  public static float floatValue1, floatValue2;
  public static double doubleValue1, doubleValue2;

  public static boolean doThrow = false;
}

