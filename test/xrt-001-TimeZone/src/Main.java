
import java.util.Arrays;
import java.util.List;
import java.util.TimeZone;

public class Main {
  static public void main(String[] args) {
    List<String> timeZones = Arrays.asList("Europe/Moscow", "America/Los_Angeles", "Europe/Copenhagen");
    
    for(String tz : timeZones)
    {
    		TimeZone timeZone = TimeZone.getTimeZone(tz);
      
    		System.out.println("getDisplayName: " + timeZone.getDisplayName());
    		System.out.println("getID: " + timeZone.getID());
    		System.out.println("getOffset: " + timeZone.getOffset(System.currentTimeMillis()));
    }
  }
}
