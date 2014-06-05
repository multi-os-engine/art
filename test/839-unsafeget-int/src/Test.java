import java.lang.reflect.Field;
import sun.misc.Unsafe;

public class Test {
    private static Unsafe unsafe;

    public int myField;

    public Test() {}
    public void run() throws Exception {

        // Since getUnsafe() is available only from trusted code
        // we should use a workaround to get it.
        Field f = Unsafe.class.getDeclaredField("theUnsafe");
        f.setAccessible(true);
        unsafe = (Unsafe) f.get(null);

        // Set a value which will be checked by compareAndSwapInt()
        myField = 66;

        // Get field descriptor of the "public int myField;"
        Field field = this.getClass().getDeclaredField("myField");

        // Now let's calculate an offset of the "public int myField;"
        long offset = unsafe.objectFieldOffset(field);

        // Test getInt()
        int i = unsafe.getInt(this, offset);

        System.out.println(i);
    }
}
