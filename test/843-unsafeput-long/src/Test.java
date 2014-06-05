import java.lang.reflect.Field;
import sun.misc.Unsafe;

public class Test {
    private static Unsafe unsafe;

    public long myField;

    public Test() {}
    public void run() throws Exception {

        // Since getUnsafe() is available only from trusted code
        // we should use a workaround to get it.
        Field f = Unsafe.class.getDeclaredField("theUnsafe");
        f.setAccessible(true);
        unsafe = (Unsafe) f.get(null);

        // Get field descriptor of the "public long myField;"
        Field field = this.getClass().getDeclaredField("myField");

        // Now let's calculate an offset of the "public long myField;"
        long offset = unsafe.objectFieldOffset(field);

        // Test putLong()
        unsafe.putLong(this, offset, -6144092013335888982L);

        System.out.println(myField);
    }
}
