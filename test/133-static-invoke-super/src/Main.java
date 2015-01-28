
public class Main {
    static private int basisTestValue = 12;

    static class SuperClass {
      protected static final long newCount(final int w, final int index) {
          return ((long) (w & 0xFF) << (index << 3));
      }
      protected static final byte extractW0(final long count) {
          return (byte) count;
      }
      protected static final byte extractW1(final long count) {
          return (byte) (count >>> 8);
      }
      protected static final byte extractW2(final long count) {
          return (byte) (count >>> 16);
      }
      protected static final byte extractWin(final long count) {
          return (byte) (count >>> 24);
      }
    }

    static class SubClass extends SuperClass {
      final long newCountDirect(final int w, final int index) {
        return ((long) (w & 0xFF) << (index << 3));
      }
      final byte extractW0Direct(final long count) {
          return (byte) count;
      }
      final byte extractW1Direct(final long count) {
          return (byte) (count >>> 8);
      }
      final byte extractW2Direct(final long count) {
          return (byte) (count >>> 16);
      }
      final byte extractWinDirect(final long count) {
          return (byte) (count >>> 24);
      }
      public void testDirect(int max) {
        final int min = 0;
        for (int b1 = min; b1 <= max; b1++) {
          for (int b2 = min; b2 <= max; b2++) {
            for (int b3 = min; b3 <= max; b3++) {
              for (int b4 = min; b4 <= max; b4++) {
                final long value = newCountDirect(b1, 0) | newCountDirect(b2, 1) |
                    newCountDirect(b3, 2) | newCountDirect(b4, 3);
                if (b1 != extractW0Direct(value) || b2 != extractW1Direct(value) ||
                    b3 != extractW2Direct(value) || b4 != extractWinDirect(value)) {
                  throw new RuntimeException("Should not happen");
                }
              }
            }
          }
        }
      }
      public void testStatic(int max) {
        final int min = 0;
        for (int b1 = min; b1 <= max; b1++) {
          for (int b2 = min; b2 <= max; b2++) {
            for (int b3 = min; b3 <= max; b3++) {
              for (int b4 = min; b4 <= max; b4++) {
                final long value = newCount(b1, 0) | newCount(b2, 1) | newCount(b3, 2) | newCount(b4, 3);
                if (b1 != extractW0(value) || b2 != extractW1(value) ||
                    b3 != extractW2(value) || b4 != extractWin(value)) {
                  throw new RuntimeException("Should not happen");
                }
              }
            }
          }
        }
      }
    }

    static public void main(String[] args) throws Exception {
        boolean timing = (args.length >= 1) && args[0].equals("--timing");
        run(timing);
    }

    static int testBasis(int interations) {
      (new SubClass()).testDirect(interations);
      return interations;
    }

    static int testStatic(int interations) {
      (new SubClass()).testStatic(interations);
      return interations;
    }

    static public void run(boolean timing) {
        long time0 = System.nanoTime();
        int count1 = testBasis(50);
        long time1 = System.nanoTime();
        int count2 = testStatic(50);
        long time2 = System.nanoTime();

        System.out.println("basis: performed " + count1 + " iterations");
        System.out.println("test1: performed " + count2 + " iterations");

        double basisMsec = (time1 - time0) / (double) count1 / 1000000;
        double msec1 = (time2 - time1) / (double) count2 / 1000000;

        if (msec1 < basisMsec * 5) {
            System.out.println("Timing is acceptable.");
        } else {
            System.out.println("Iterations are taking too long!");
            timing = true;
        }
        if (timing) {
            System.out.printf("basis time: %.3g msec\n", basisMsec);
            System.out.printf("test1: %.3g msec per iteration\n", msec1);
        }
    }
}
