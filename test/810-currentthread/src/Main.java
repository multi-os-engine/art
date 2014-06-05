public class Main implements Runnable {

    Main() {
        Thread currThread = Thread.currentThread();
        System.out.println(currThread);
    }

    public void run() {
    }

    public static void main(String args[]) {
        new Main();
    }
}