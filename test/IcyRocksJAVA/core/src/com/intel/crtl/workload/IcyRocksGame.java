package com.intel.crtl.workload;

import com.badlogic.gdx.Game;
import com.badlogic.gdx.Gdx;
import com.badlogic.gdx.graphics.FPSLogger;
import com.badlogic.gdx.graphics.OrthographicCamera;
import com.badlogic.gdx.graphics.g2d.SpriteBatch;
import com.badlogic.gdx.graphics.g2d.TextureAtlas;
import com.badlogic.gdx.utils.viewport.FitViewport;
import com.badlogic.gdx.utils.viewport.Viewport;

/**
 * Created by wfeng on 2/22/16.
 */
public class IcyRocksGame extends Game {

    public SpriteBatch batch;
    public FPSLogger fpsLogger;
    public OrthographicCamera camera;
    public TextureAtlas atlas;
    public Viewport viewport;
    public static final int screenWidth = 1368;
    public static final int screenHeight = 720;

    public IcyRocksGame() {
        fpsLogger = new FPSLogger();
        camera = new OrthographicCamera();
        camera.position.set(screenWidth/2, screenHeight/2, 0);
        viewport = new FitViewport(screenWidth, screenHeight, camera);
    }

    @Override
    public void create() {
        batch = new SpriteBatch();
        atlas = new TextureAtlas(Gdx.files.internal("icyworld.atlas"));
        setScreen(new IcyRocksScene(this));
    }

    @Override
    public void resize(int w, int h) {
        viewport.update(w, h);
    }

    @Override
    public void dispose() {
        super.dispose();
        batch.dispose();
        atlas.dispose();
    }
}
