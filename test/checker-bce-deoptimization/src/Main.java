public class Main {
    public static void main(String[] args) {
        new Main().run();
        System.out.println("finish");
    }

    public void run() {
        double a[][] = new double[200][201];
        double b[] = new double[200];
        int n = 100;

        foo1(a,n,b);
    }

    void foo1(double a[][], int n, double b[]) {
        double t;
        int i,k;

        for (i = 0; i < n; i++) {
            k = n - (i + 1);
            b[k] /= a[k][k];
            t = -b[k];
            foo2(k+1000,t,b);
        }
    }

    void foo2(int n, double c, double b[]) {
        try {
            foo3(n,c,b);
        } catch (Exception e) {
        }
    }

    void foo3(int n, double c, double b[]) {
        int i = 0;
        for (i=0; i < n; i++) {
            b[i + 1] += c * b[i + 1];
        }
    }
}

