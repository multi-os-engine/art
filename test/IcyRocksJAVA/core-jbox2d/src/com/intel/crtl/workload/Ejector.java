package com.intel.crtl.workload;

import com.badlogic.gdx.graphics.g2d.SpriteBatch;
import com.badlogic.gdx.math.MathUtils;

import org.jbox2d.common.Vec2;
import org.jbox2d.dynamics.BodyType;
import org.jbox2d.dynamics.World;
import org.jbox2d.dynamics.joints.RevoluteJoint;
import org.jbox2d.dynamics.joints.RevoluteJointDef;
import org.jbox2d.dynamics.joints.WeldJoint;
import org.jbox2d.dynamics.joints.WeldJointDef;

/**
 * Created by wfeng on 2/25/16.
 */
public class Ejector {
    private IcyWorldSprite base;
    private IcyWorldSprite arm;
    private RevoluteJoint joint;
    private WeldJoint stoneJoint;
    private IcyWorldSprite stone;
    private boolean removeStone = false;

    public Ejector(SpriteFactory factory, World world, float factor) {
        base = factory.create("ejectorbase",
                new Vec2(0, IcyRocksScene.EjectorHeight),
                BodyType.STATIC, 0f, .2f, .8f,
                SpriteFactory.SpriteShapes.Custom);
        arm = factory.create("ejectorarm",
                new Vec2(IcyRocksScene.EjectorLength, IcyRocksScene.EjectorHeight),
                BodyType.DYNAMIC, 1.0f, .2f, .8f,
                SpriteFactory.SpriteShapes.Custom);
        joint = createJoint(world, factor);
    }

    public void update(World world) {
        // check stoneJoint
        if (stoneJoint != null
                && stoneJoint.getBodyA().getLinearVelocity().x > 0
                && joint.getJointAngle() <= 20 * MathUtils.degreesToRadians) {
            world.destroyJoint(stoneJoint);
            stoneJoint = null;
        }

        if (stone != null && removeStone) {
            if (stoneJoint != null && stoneJoint.getBodyA() == stone.getBody()) {
                world.destroyJoint(stoneJoint);
                stoneJoint = null;
            }

            world.destroyBody(stone.getBody());
            stone = null;
        }

    }

    public void draw(SpriteBatch batch) {
        base.draw(batch);
        arm.draw(batch);
        if (stone != null) {
            stone.draw(batch);
        }
    }

    public void fire(SpriteFactory factory, World world) {
        stone = factory.create("stone",
                new Vec2(IcyRocksScene.EjectorLength,
                        IcyRocksScene.EjectorHeight + IcyRocksScene.EjectorLength),
                BodyType.DYNAMIC, 1.5f, .2f, .8f,
                SpriteFactory.SpriteShapes.Custom);
        WeldJointDef jointDef = new WeldJointDef();
        jointDef.initialize(stone.getBody(), arm.getBody(),
                stone.getBody().getPosition());
        jointDef.collideConnected = false;
        stoneJoint = (WeldJoint) world.createJoint(jointDef);
        arm.getBody().applyAngularImpulse(1000);
        removeStone = false;
    }

    public IcyWorldSprite getStone() { return stone; }

    public void removeStone() {
        removeStone = true;
    }

    private RevoluteJoint createJoint(World world, float factor) {
        RevoluteJointDef jointDef = new RevoluteJointDef();
        Vec2 anchor = new Vec2(IcyRocksScene.EjectorLength / factor,
                IcyRocksScene.EjectorHeight / factor);
        jointDef.initialize(base.getBody(), arm.getBody(), anchor);
        jointDef.enableMotor = true;
        jointDef.motorSpeed = -15f;
        jointDef.maxMotorTorque = 700f;
        jointDef.enableLimit = true;
        jointDef.lowerAngle = 15 * MathUtils.degreesToRadians;
        jointDef.upperAngle = 45 * MathUtils.degreesToRadians;
        return (RevoluteJoint) world.createJoint(jointDef);
    }
}
