package com.intel.crtl.workload;

import com.badlogic.gdx.graphics.g2d.TextureAtlas;
import com.badlogic.gdx.math.Vector2;
import com.badlogic.gdx.physics.box2d.Body;
import com.badlogic.gdx.physics.box2d.BodyDef;
import com.badlogic.gdx.physics.box2d.ChainShape;
import com.badlogic.gdx.physics.box2d.CircleShape;
import com.badlogic.gdx.physics.box2d.EdgeShape;
import com.badlogic.gdx.physics.box2d.Fixture;
import com.badlogic.gdx.physics.box2d.FixtureDef;
import com.badlogic.gdx.physics.box2d.PolygonShape;
import com.badlogic.gdx.physics.box2d.World;

import aurelienribon.bodyeditor.BodyEditorLoader;

/**
 * Created by wfeng on 2/26/16.
 */
public class SpriteFactory {
    private World world;
    private BodyEditorLoader loader;
    private TextureAtlas atlas;
    private float factor;

    public enum SpriteShapes {
        Circle,
        Chain,
        Edge,
        Rectangle,
        Custom
    }

    public SpriteFactory(World w, BodyEditorLoader l, TextureAtlas a, float factor) {
        world = w;
        loader = l;
        atlas = a;
        this.factor = factor;
    }

    public IcyWorldSprite create(String name, Vector2 pos, BodyDef.BodyType type,
                                 float density, float friction, float restitution,
                                 SpriteShapes shape) {
        BodyDef bodyDef = new BodyDef();
        bodyDef.type = type;
        bodyDef.position.x = pos.x / factor;
        bodyDef.position.y = pos.y / factor;
        Body body = world.createBody(bodyDef);

        IcyWorldSprite sprite = new IcyWorldSprite(atlas.findRegion(name), body, factor);
        body.setUserData(sprite);

        FixtureDef fd = new FixtureDef();
        fd.density = density;
        fd.friction = friction;
        fd.restitution = restitution;

        switch (shape) {
            case Circle:
                CircleShape cs = new CircleShape();
                cs.setRadius(sprite.getRegionWidth() / (factor * 2));
                fd.shape = cs;
                body.createFixture(fd);
                cs.dispose();
                break;
            case Chain:
                ChainShape chainShape = new ChainShape();
                Vector2[] vertices = new Vector2[4];
                float y = IcyRocksScene.GroundHeight / (2 * factor);
                float h = (IcyRocksScene.SceneHeight - IcyRocksScene.GroundHeight / 2) / factor;
                float w = IcyRocksScene.SceneWidth / (2 * factor);
                vertices[0] = new Vector2(-w, y);
                vertices[1] = new Vector2( w, y);
                vertices[2] = new Vector2( w, h);
                vertices[3] = new Vector2(-w, h);

                chainShape.createChain(vertices);
                Fixture sideFixture = body.createFixture(chainShape, 0.0f);
                sideFixture.setFriction(0.01f);
                chainShape.dispose();
                break;
            case Edge:
                EdgeShape edge = new EdgeShape();
                edge.set(new Vector2(0, IcyRocksScene.SceneHeight / factor),
                        new Vector2(0, IcyRocksScene.GroundHeight / factor));
                Fixture fixture = body.createFixture(edge, 0f);
                fixture.setFriction(.01f);
                edge.dispose();
                break;
            case Rectangle:
                PolygonShape rect = new PolygonShape();
                rect.setAsBox(sprite.getWidth() / (factor * 2), sprite.getHeight() / (factor * 2));
                fd.shape = rect;
                body.createFixture(fd);
                rect.dispose();
                break;
            case Custom:
                loader.attachFixture(body, name, fd, sprite.getRegionWidth() / factor);
                break;
        }

        return sprite;
    }

}
