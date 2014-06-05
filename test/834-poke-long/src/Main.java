import libcore.io.Memory;
import dalvik.system.VMRuntime;

public class Main {

    public static void main(String[] args) {
        byte[] b = new byte [8];
        VMRuntime runtime = VMRuntime.getRuntime();
        long address = runtime.addressOf(b);
        Memory.pokeLong(address,578437695752307201L,false);
        System.out.println(b[0]);
        System.out.println(b[1]);
        System.out.println(b[2]);
        System.out.println(b[3]);
        System.out.println(b[4]);
        System.out.println(b[5]);
        System.out.println(b[6]);
        System.out.println(b[7]);
    }
}
