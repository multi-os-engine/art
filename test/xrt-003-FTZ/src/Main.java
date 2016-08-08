public class Main {
  public static void main(String args[]) throws Exception {

    Thread t = new Thread(new Runnable() {
        public void run() {
            checkFTZ(" in thread");   
        }
    });
    t.start();
    t.join();

    //checking main thread as well
    checkFTZ("");
    System.out.print("Done!");
  }

  private static void checkFTZ(String isThread)  {

      Float v1 = Math.min(Float.MIN_VALUE, 0.0f);
      if ( !v1.equals(0.0f)) {
        System.out.println("ERROR: FTZ enabled" + isThread + ", since min(" + Float.valueOf(Float.MIN_VALUE) + ", 0.0f) != 0.0f");
      }
  }
}
