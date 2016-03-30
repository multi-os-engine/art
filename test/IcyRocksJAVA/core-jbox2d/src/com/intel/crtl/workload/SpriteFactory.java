package com.intel.crtl.workload;

import com.badlogic.gdx.graphics.g2d.TextureAtlas;

import org.jbox2d.collision.shapes.ChainShape;
import org.jbox2d.collision.shapes.CircleShape;
import org.jbox2d.collision.shapes.EdgeShape;
import org.jbox2d.collision.shapes.PolygonShape;
import org.jbox2d.common.Vec2;
import org.jbox2d.dynamics.Body;
import org.jbox2d.dynamics.BodyDef;
import org.jbox2d.dynamics.BodyType;
import org.jbox2d.dynamics.Fixture;
import org.jbox2d.dynamics.FixtureDef;
import org.jbox2d.dynamics.World;

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

    public IcyWorldSprite create(String name, Vec2 pos, BodyType type,
                                 float density, float friction, float restitution,
                                 SpriteShapes shape) {
        BodyDef bodyDef = new BodyDef();
        bodyDef.type = type;
        bodyDef.position.set(pos.x / factor, pos.y / factor);
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
                break;
            case Chain:
                ChainShape chainShape = new ChainShape();
                Vec2[] vertices = new Vec2[4];
                float b = IcyRocksScene.GroundHeight / factor;
                float t = IcyRocksScene.SceneHeight / factor;
                float w = IcyRocksScene.SceneWidth / factor;
                vertices[0] = new Vec2(0, b);
                vertices[1] = new Vec2(w, b);
                vertices[2] = new Vec2(w, t);
                vertices[3] = new Vec2(0, t);

                chainShape.createChain(vertices, vertices.length);
                Fixture sideFixture = body.createFixture(chainShape, 0.0f);
                sideFixture.setFriction(0.01f);
                break;
            case Edge:
                EdgeShape edge = new EdgeShape();
                edge.set(new Vec2(0, IcyRocksScene.SceneHeight / factor),
                        new Vec2(0, IcyRocksScene.GroundHeight / factor));
                Fixture fixture = body.createFixture(edge, 0f);
                fixture.setFriction(.01f);
                break;
            case Rectangle:
                PolygonShape rect = new PolygonShape();
                rect.setAsBox(sprite.getWidth() / (factor * 2), sprite.getHeight() / (factor * 2));
                fd.shape = rect;
                body.createFixture(fd);
                break;
            case Custom:
                loader.attachFixture(body, name, fd, sprite.getRegionWidth() / factor);
                break;
        }

        return sprite;
    }

}
