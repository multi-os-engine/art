import libcore.io.Memory;
import dalvik.system.VMRuntime;

public class Main {

    public static void main(String[] args) {
        byte[] b = new byte [2];
        VMRuntime runtime = VMRuntime.getRuntime();
        long address = runtime.addressOf(b);
        Memory.pokeShort(address,(short)513,false);
        System.out.println(b[0]);
        System.out.println(b[1]);
    }
}
