public class Main {
  public static void main(String[] args) {
    Object[] a = new Object[4];
    for (int i = 0; i < 4; i++) {
      a[i] = new Integer(i);
    }
    $noinline$callArrayCopy(a, a);

    expectEquals(0, ((Integer)a[0]).intValue());
    expectEquals(0, ((Integer)a[1]).intValue());
    expectEquals(1, ((Integer)a[2]).intValue());
    expectEquals(3, ((Integer)a[3]).intValue());
  }

  public static void expectEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  public static void $noinline$callArrayCopy(Object[] a, Object[] b) {
    System.arraycopy(a, 0, b, 1, 2);
    if (doThrow) { throw new Error(""); }
  }

  static boolean doThrow = false;
}
