public class Main {

    public static void main(String args[]) {
        String s1 = "Vegetables are tasty";
        String s2 = "Vegetables are tasty";
        String s3 = "Fruits are not tasty";

        int r = s1.compareTo( s2 );
        System.out.println(r);

        r = s2.compareTo( s3 );
        System.out.println(r);

        r = s3.compareTo( s1 );
        System.out.println(r);
    }
}
