public class Foo {

  public static void doNothing(boolean b) {
    System.out.println("On do nothin");
  }

  public static void inIf() {
    System.out.println("in if");
  }

  public static int bar() {
    return 42;
  }

  public static int foo() {
    int b = bar();
    doNothing(b == 42);
    if (b == 42) {
      inIf();
      return 42;
    }
    return 0;
  }

  public static void main(String[] args){ 
    System.out.println(foo());
  }
}
