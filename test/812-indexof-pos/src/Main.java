public class Main {

    public static int foo(int pos)
    {
        String s = new String("Hello world!");
        return s.indexOf( 'o', pos);
    }

    public static void main(String args[]) {
        String s = new String("Hello world!");
        String ss1 = new String("ello");
        String ss2 = new String("forld");
        char c = 'w';

        int pos = s.indexOf( 'l', 3);
        System.out.println(pos);
        System.out.println(s.indexOf( ss1, 1 ));
        System.out.println(s.indexOf( ss2, 2 ));
        System.out.println(s.indexOf( 'l', pos + 1));
        System.out.println(foo(1));
        pos = s.indexOf( c, 3);
        System.out.println(pos);
        System.out.println(s.indexOf( c, pos - 1));
    }
}
