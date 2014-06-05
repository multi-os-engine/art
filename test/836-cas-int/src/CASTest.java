import java.lang.reflect.Field;
import sun.misc.Unsafe;

public class CASTest {
    private static Unsafe unsafe;

    public int myField;

    public CASTest() {}
    public void run() throws Exception {

        // Since getUnsafe() is available only from trusted code
        // we should use a workaround to get it.
        Field f = Unsafe.class.getDeclaredField("theUnsafe");
        f.setAccessible(true);
        unsafe = (Unsafe) f.get(null);

        // Set a value which will be checked by compareAndSwapInt()
        myField = 15;

        // Get field descriptor of the "public int myField;"
        Field field = this.getClass().getDeclaredField("myField");

        // Now let's calculate an offset of the "public int myField;"
        long offset = unsafe.objectFieldOffset(field);

        // Test compareAndSwapInt()
        unsafe.compareAndSwapInt(this, offset, 15, 66);

        System.out.println(myField);
    }
}
