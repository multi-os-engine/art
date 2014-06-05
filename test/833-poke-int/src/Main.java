import libcore.io.Memory;
import dalvik.system.VMRuntime;

public class Main {

    public static void main(String[] args) {
        byte[] b = new byte [4];
        VMRuntime runtime = VMRuntime.getRuntime();
        long address = runtime.addressOf(b);
        Memory.pokeInt(address,67305985,false);
        System.out.println(b[0]);
        System.out.println(b[1]);
        System.out.println(b[2]);
        System.out.println(b[3]);
    }
}
