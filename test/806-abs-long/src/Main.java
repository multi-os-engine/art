import java.lang.Math;

public class Main {

    public static void main(String[] args) {

        long x = 9223372036854775807L;
        long y = 0L;

        System.out.println(Math.abs(x));
        System.out.println(Math.abs(-y));
        System.out.println(Math.abs(-9223372036854775807L));
        System.out.println(Math.abs(-9223372036854775808L));
    }
}
