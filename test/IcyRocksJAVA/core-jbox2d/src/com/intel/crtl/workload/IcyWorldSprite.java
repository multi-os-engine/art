package com.intel.crtl.workload;

import com.badlogic.gdx.graphics.g2d.Sprite;
import com.badlogic.gdx.graphics.g2d.SpriteBatch;
import com.badlogic.gdx.graphics.g2d.TextureRegion;
import com.badlogic.gdx.math.MathUtils;

import org.jbox2d.dynamics.Body;
import org.jbox2d.dynamics.BodyType;

/**
 * Created by wfeng on 3/08/16.
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
        this.visible = true;
    }

    public void draw(SpriteBatch batch) {
        if (body.getType() == BodyType.STATIC) {
            setPosition(body.getPosition().x * factor,
                        body.getPosition().y * factor);
        } else {
            setCenter(body.getWorldCenter().x * factor,
                      body.getWorldCenter().y * factor);
            setRotation(body.getAngle() * MathUtils.radiansToDegrees);
        }
        if (isVisible())
            super.draw(batch);
    }

    public Body getBody() {
        return body;
    }

    public boolean isVisible() {
        return visible;
    }

    public void setVisible(boolean v) {
        visible = v;
    }
}
