import java.lang.Long;

public class Main {

    public static void main(String args[]) {
        long a = 0xFFAAEEDDL;
        System.out.println(Long.reverseBytes(a));
        System.out.println(Long.reverseBytes(0xFFAAEEBBL));
    }
}
