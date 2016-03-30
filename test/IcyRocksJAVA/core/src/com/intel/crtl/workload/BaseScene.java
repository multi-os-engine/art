package com.intel.crtl.workload;

import com.badlogic.gdx.InputProcessor;
import com.badlogic.gdx.ScreenAdapter;

/**
 * Created by wfeng on 2/24/16.
 */
public class BaseScene extends ScreenAdapter implements InputProcessor {
    protected IcyRocksGame game;
    public BaseScene(IcyRocksGame game) {
        this.game = game;
    }

    @Override
    public void render(float delta) {
        super.render(delta);
    }

    @Override
    public boolean keyDown(int keycode) {
        return false;
    }

    @Override
    public boolean keyUp(int keycode) {
        return false;
    }

    @Override
    public boolean keyTyped(char character) {
        return false;
    }

    @Override
    public boolean touchDown(int screenX, int screenY, int pointer, int button) {
        return false;
    }

    @Override
    public boolean touchUp(int screenX, int screenY, int pointer, int button) {
        return false;
    }

    @Override
    public boolean touchDragged(int screenX, int screenY, int pointer) {
        return false;
    }

    @Override
    public boolean mouseMoved(int screenX, int screenY) {
        return false;
    }

    @Override
    public boolean scrolled(int amount) {
        return false;
    }
}
