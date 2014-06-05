import libcore.io.Memory;
import dalvik.system.VMRuntime;

public class Main {

    public static void main(String[] args) {
        byte[] b = new byte [1];
        b[0] = 13;
        VMRuntime runtime = VMRuntime.getRuntime();
        long address = runtime.addressOf(b);
        System.out.println(Memory.peekByte(address));
    }
}
