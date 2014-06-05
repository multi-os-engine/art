import java.lang.Short;

public class Main {

    public static void main(String args[]) {
        short a = -2;
        System.out.println(Short.reverseBytes(a));
        System.out.println(Short.reverseBytes((short) -32767));
    }
}
