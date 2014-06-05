import java.lang.reflect.Field;
import sun.misc.Unsafe;

public class CASTest {
    private static Unsafe unsafe;

    public MyObject myField;

    public CASTest() {}
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

        newObj.x = 10;
        newObj.y = 11L;

        unsafe.compareAndSwapObject(this, offset, myField, newObj);

        System.out.println(myField.x);
        System.out.println(myField.y);
    }
}
