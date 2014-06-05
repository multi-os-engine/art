import libcore.io.Memory;
import dalvik.system.VMRuntime;

public class Main {

    public static void main(String[] args) {
        byte[] b = new byte [1];
        b[0] = 0;
        VMRuntime runtime = VMRuntime.getRuntime();
        long address = runtime.addressOf(b);
        Memory.pokeByte(address,(byte )13);
        System.out.println(b[0]);
    }
}
