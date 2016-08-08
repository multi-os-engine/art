import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.ArrayList;
import java.util.Collections;

public class Main {
  static public void main(String[] args) throws SocketException {
    ArrayList<NetworkInterface> nifs = Collections.list(NetworkInterface.getNetworkInterfaces());
    
    assert(nifs != null);
    
    System.out.println("Done!");
  }
}
