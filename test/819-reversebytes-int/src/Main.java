import java.lang.Integer;

public class Main {

    public static void main(String args[]) {
        int a = 0x0000ffaa;
        System.out.println(Integer.reverseBytes(a));
        System.out.println(Integer.reverseBytes(0xccbbffaa));
    }
}
