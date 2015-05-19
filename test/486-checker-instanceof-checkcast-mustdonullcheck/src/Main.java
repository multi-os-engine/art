public class Main {
  // CHECK-START: void Main.InstanceOfPreChecked(java.lang.Object) instruction_simplifier (after)
  // CHECK:       can_be_null:0
  public void InstanceOfPreChecked(Object o) throws Exception {
    o.toString();
    if (o instanceof Main) {
      throw new Exception();
    }
  }

  // CHECK-START: void Main.InstanceOf(java.lang.Object) instruction_simplifier (after)
  // CHECK:       can_be_null:1
  public void InstanceOf(Object o) throws Exception {
    if (o instanceof Main) {
      throw new Exception();
    }
  }

  // CHECK-START: void Main.CheckCastPreChecked(java.lang.Object) instruction_simplifier (after)
  // CHECK:       can_be_null:0
  public void CheckCastPreChecked(Object o) {
    o.toString();
    ((Main)o).Bar();
  }

  // CHECK-START: void Main.CheckCast(java.lang.Object) instruction_simplifier (after)
  // CHECK:       can_be_null:1
  public void CheckCast(Object o) {
    ((Main)o).Bar();
  }

  void Bar() {throw new RuntimeException();}

  public static void main(String[] sa) {
    Main t = new Main();
  }
}
