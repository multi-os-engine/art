import java.lang.reflect.Method;

public class Main {
    static char [][] holder;
    static boolean sawOome;

    static void blowup() {
        try {
            for (int i = 0; i < holder.length; ++i) {
                holder[i] = new char[1024 * 1024];
            }
        } catch (OutOfMemoryError oome) {
            sawOome = true;
        }
    }

    public static void main(String args[]) throws Exception {
        Class<?> c = Class.forName("Test");
        Method m = c.getMethod("run", (Class[]) null);
        for (int i = 0; i < 10; i++) {
            holder = new char[128 * 1024][];
            m.invoke(null, (Object[]) null);
            holder = null;
        }
    }
}
