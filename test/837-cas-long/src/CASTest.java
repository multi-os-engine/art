import java.lang.reflect.Field;
import sun.misc.Unsafe;

public class CASTest {
    private static Unsafe unsafe;

    public long myField;

    public CASTest() {}
    public void run() throws Exception {

        Field f = Unsafe.class.getDeclaredField("theUnsafe");
        f.setAccessible(true);
        unsafe = (Unsafe) f.get(null);

        myField = -6144092017057071104L;

        Field field = this.getClass().getDeclaredField("myField");

        long offset = unsafe.objectFieldOffset(field);

        unsafe.compareAndSwapLong(this, offset, -6144092017057071104L, -6144092013335888982L);

        System.out.println(myField);
    }
}
