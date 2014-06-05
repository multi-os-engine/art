import libcore.io.Memory;
import dalvik.system.VMRuntime;

public class Main {

    public static void main(String[] args) {
        byte[] b = new byte [4];
        b[0] = 1;
        b[1] = 2;
        b[2] = 3;
        b[3] = 4;
        VMRuntime runtime = VMRuntime.getRuntime();
        long address = runtime.addressOf(b);
        System.out.println(Memory.peekInt(address,false));
    }
}
