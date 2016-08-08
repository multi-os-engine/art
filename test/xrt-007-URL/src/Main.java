import java.net.URL;
import java.io.*;

public class Main {
  public static void main(String args[]) {

    try {
      System.setProperty("http.proxyHost", "proxy01.inn.intel.com");
      System.setProperty("http.proxyPort", "911");
      System.setProperty("https.proxyHost", "proxy01.inn.intel.com");
      System.setProperty("https.proxyPort", "911");

      //URL url = new URL("http://127.0.0.1:8000");
      URL url = new URL("https://www.ietf.org/rfc/rfc0159.txt");
      BufferedReader reader = new BufferedReader(new InputStreamReader(url.openStream()));
      
      
      String inputLine;
      while ((inputLine = reader.readLine()) != null) {
        System.out.println(inputLine);
      }
      reader.close();
      
      System.out.println("Done!");
    } catch (Exception e) {
      System.out.println("Caught exception: " + e.getMessage());
    }
  }
}
