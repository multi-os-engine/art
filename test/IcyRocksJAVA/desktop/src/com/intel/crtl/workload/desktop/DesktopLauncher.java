package com.intel.crtl.workload.desktop;

import com.badlogic.gdx.backends.lwjgl.LwjglApplication;
import com.badlogic.gdx.backends.lwjgl.LwjglApplicationConfiguration;
import com.intel.crtl.workload.IcyRocksGame;

public class DesktopLauncher {
    public static void main(String[] arg) {
        LwjglApplicationConfiguration config = new LwjglApplicationConfiguration();
        config.width = 1368;
        config.height = 720;
        new LwjglApplication(new IcyRocksGame(), config);
    }
}
