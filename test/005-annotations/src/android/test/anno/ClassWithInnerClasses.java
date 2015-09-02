package android.test.anno;

public class ClassWithInnerClasses {
  public class InnerClass {
    public String toString() {
      return "Canonical:" + getClass().getCanonicalName() + " Simple:" + getClass().getSimpleName();
    }
  }
  Object anonymousClass = new Object() {
    public String toString() {
      return "Canonical:" + getClass().getCanonicalName() + " Simple:" + getClass().getSimpleName();
    }
  };

  public void print() {
    System.out.println(new InnerClass());
    System.out.println(anonymousClass);
  }
}
