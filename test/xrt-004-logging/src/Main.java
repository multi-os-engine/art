
import java.util.logging.*;
import java.io.IOException;
import java.io.File;
import java.io.*;

public class Main {
    public static void logFile() throws IOException {
        String fileName = "/tmp/log_004_logging.txt";
        Logger logger=Logger.getLogger("xrt_004_logging.Main");
        FileHandler h;
        
        try {
            h = new FileHandler(fileName);
            logger.addHandler(h);
        } catch (IOException x){
            fileName = System.getenv("TMPDIR")+"/log_004_logging.txt";
            h = new FileHandler(fileName);
            logger.addHandler(h);
        }
        
        h.setFormatter(new SimpleFormatter());
        
        System.out.println("");
        System.out.println("Start looging in file");
        
        logger.setLevel(Level.INFO);
        logger.info("START");
        logger.log(Level.WARNING, "warning1");
        logger.warning("warning2");
        logger.log(Level.SEVERE, "severe");
        logger.info("FINISH");
        
        logger.setLevel(Level.SEVERE);
        logger.info("START");
        logger.log(Level.WARNING, "warning1");
        logger.warning("warning2");
        logger.log(Level.SEVERE, "severe");
        logger.info("FINISH");
        
        h.close();
        
        FileReader fr = new FileReader(fileName);
        BufferedReader br = new BufferedReader(fr);
        String line = null;
        
        System.out.println("");
        System.out.println("File output:");
        while((line = br.readLine()) != null){
            System.out.println(line);
        }
        
    }
    
    public static void main(String[] args) throws IOException {
          Logger logger=Logger.getLogger("xrt_004_logging.Main");
        
          logger.info("START");
          logger.log(Level.WARNING, "warning1");
          logger.warning("warning2");
          logger.log(Level.SEVERE, "severe");
          logger.info("FINISH");
        
          logger.setLevel(Level.SEVERE);
          logger.info("START");
          logger.log(Level.WARNING, "warning1");
          logger.warning("warning2");
          logger.log(Level.SEVERE, "severe");
          logger.info("FINISH");
        
          logFile();
    }
  }
