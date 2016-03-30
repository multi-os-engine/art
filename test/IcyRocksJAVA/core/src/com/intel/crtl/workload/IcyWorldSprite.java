package com.intel.crtl.workload;

import com.badlogic.gdx.graphics.g2d.Sprite;
import com.badlogic.gdx.graphics.g2d.SpriteBatch;
import com.badlogic.gdx.graphics.g2d.TextureRegion;
import com.badlogic.gdx.math.MathUtils;
import com.badlogic.gdx.physics.box2d.Body;

/**
 * Created by wfeng on 2/25/16.
 */
public class IcyWorldSprite extends Sprite {
    private float factor;
    private Body body;
    private boolean visible;

    public IcyWorldSprite(TextureRegion reg, Body body, float factor) {
        super(reg);
        this.body = body;
        this.factor = factor;
        this.body.setUserData(this);
    }

    public void draw(SpriteBatch batch) {
        setPosition(body.getPosition().x * factor - getRegionWidth() / 2,
                body.getPosition().y * factor - getRegionHeight() / 2);
        setRotation(body.getAngle() * MathUtils.radiansToDegrees);
        super.draw(batch);
    }

    public Body getBody() {
        return body;
    }

    public void setBody(Body b) {
        body = b;
        body.setUserData(this);
    }

    public boolean isVisible() {
        return visible;
    }

    public void setVisible(boolean v) {
        visible = v;
    }
}
