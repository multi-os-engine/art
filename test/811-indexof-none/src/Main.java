public class Main {

    public static int foo(char c)
    {
        String s = new String("Hello world!");
        return s.indexOf(c);
    }

    public static void main(String args[]) {
        String s = new String("Hello world!");
        String ss1 = new String("ello");
        String ss2 = new String("forld");
        char c = 'w';

        System.out.println(s.indexOf( 'o' ));
        System.out.println(s.indexOf( ss1 ));
        System.out.println(s.indexOf( ss2 ));
        System.out.println(foo('o'));
        System.out.println(s.indexOf(c));
    }
}
