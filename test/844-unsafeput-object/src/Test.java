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

        Field field = this.getClass().getDeclaredField("myField");

        long offset = unsafe.objectFieldOffset(field);

        MyObject newObj = new MyObject();
        newObj.x = 15;
        newObj.y = -6144092017057071104L;

        unsafe.putObject(this, offset, newObj);

        System.out.println(myField.x);
        System.out.println(myField.y);
    }
}
