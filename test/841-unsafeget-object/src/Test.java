import java.lang.reflect.Field;
import sun.misc.Unsafe;

public class Test {
    private static Unsafe unsafe;

    public MyObject myField;

    public Test() {}
    public void run() throws Exception {

        Field f = Unsafe.class.getDeclaredField("theUnsafe");
        f.setAccessible(true);
        unsafe = (Unsafe) f.get(null);

        myField = new MyObject();
        myField.x = 15;
        myField.y = -6144092017057071104L;

        Field field = this.getClass().getDeclaredField("myField");

        long offset = unsafe.objectFieldOffset(field);

        MyObject newObj = new MyObject();

        newObj = (MyObject )unsafe.getObject(this, offset);

        System.out.println(newObj.x);
        System.out.println(newObj.y);
    }
}
