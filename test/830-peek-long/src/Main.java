import libcore.io.Memory;
import dalvik.system.VMRuntime;

public class Main {

    public static void main(String[] args) {
        byte[] b = new byte [8];
        b[0] = 1;
        b[1] = 2;
        b[2] = 3;
        b[3] = 4;
        b[4] = 5;
        b[5] = 6;
        b[6] = 7;
        b[7] = 8;
        VMRuntime runtime = VMRuntime.getRuntime();
        long address = runtime.addressOf(b);
        System.out.println(Memory.peekLong(address,false));
    }
}
