/*******************************************************************************
 * Copyright 2011 See AUTHORS file.
 * <p/>
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * <p/>
 * http://www.apache.org/licenses/LICENSE-2.0
 * <p/>
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

package com.intel.crtl.workload;

import com.badlogic.gdx.graphics.Color;
import com.badlogic.gdx.graphics.glutils.ShapeRenderer;
import com.badlogic.gdx.math.Matrix4;
import com.badlogic.gdx.utils.Disposable;

import org.jbox2d.collision.WorldManifold;
import org.jbox2d.collision.shapes.ChainShape;
import org.jbox2d.collision.shapes.CircleShape;
import org.jbox2d.collision.shapes.EdgeShape;
import org.jbox2d.collision.shapes.PolygonShape;
import org.jbox2d.collision.shapes.ShapeType;
import org.jbox2d.common.Transform;
import org.jbox2d.common.Vec2;
import org.jbox2d.dynamics.Body;
import org.jbox2d.dynamics.BodyType;
import org.jbox2d.dynamics.Fixture;
import org.jbox2d.dynamics.World;
import org.jbox2d.dynamics.contacts.Contact;
import org.jbox2d.dynamics.joints.Joint;
import org.jbox2d.dynamics.joints.JointType;
import org.jbox2d.dynamics.joints.PulleyJoint;


public class Box2DDebugRenderer implements Disposable {

    /**
     * the immediate mode renderer to output our debug drawings
     **/
    protected ShapeRenderer renderer;

    /**
     * vertices for polygon rendering
     **/
    private final static Vec2[] vertices = new Vec2[1000];

    private final static Vec2 lower = new Vec2();
    private final static Vec2 upper = new Vec2();

    private boolean drawBodies;
    private boolean drawJoints;
    private boolean drawAABBs;
    private boolean drawInactiveBodies;
    private boolean drawVelocities;
    private boolean drawContacts;

    public Box2DDebugRenderer() {
        this(true, true, false, true, false, true);
    }

    public Box2DDebugRenderer(boolean drawBodies, boolean drawJoints, boolean drawAABBs, boolean drawInactiveBodies,
                              boolean drawVelocities, boolean drawContacts) {
        // next we setup the immediate mode renderer
        renderer = new ShapeRenderer();

        // initialize vertices array
        for (int i = 0; i < vertices.length; i++)
            vertices[i] = new Vec2();

        this.drawBodies = drawBodies;
        this.drawJoints = drawJoints;
        this.drawAABBs = drawAABBs;
        this.drawInactiveBodies = drawInactiveBodies;
        this.drawVelocities = drawVelocities;
        this.drawContacts = drawContacts;
    }

    /**
     * This assumes that the projection matrix has already been set.
     */
    public void render(World world, Matrix4 projMatrix) {
        renderer.setProjectionMatrix(projMatrix);
        renderBodies(world);
    }

    public final Color SHAPE_NOT_ACTIVE = new Color(0.5f, 0.5f, 0.3f, 1);
    public final Color SHAPE_STATIC = new Color(0.5f, 0.9f, 0.5f, 1);
    public final Color SHAPE_KINEMATIC = new Color(0.5f, 0.5f, 0.9f, 1);
    public final Color SHAPE_NOT_AWAKE = new Color(0.6f, 0.6f, 0.6f, 1);
    public final Color SHAPE_AWAKE = new Color(0.9f, 0.7f, 0.7f, 1);
    public final Color JOINT_COLOR = new Color(0.5f, 0.8f, 0.8f, 1);
    public final Color AABB_COLOR = new Color(1.0f, 0, 1.0f, 1f);
    public final Color VELOCITY_COLOR = new Color(1.0f, 0, 0f, 1f);

    private void renderBodies(World world) {
        renderer.begin(ShapeRenderer.ShapeType.Line);

        if (drawBodies || drawAABBs) {
            Body body = world.getBodyList();
            while (body != null) {
                if (body.isActive() || drawInactiveBodies) renderBody(body);
                body = body.getNext();
            }
        }

        if (drawJoints) {
            Joint joint = world.getJointList();
            while (joint != null) {
                drawJoint(joint);
                joint = joint.getNext();
            }
        }
        renderer.end();
        if (drawContacts) {
            renderer.begin(ShapeRenderer.ShapeType.Point);
            Contact contact = world.getContactList();
            while (contact != null) {
                drawContact(contact);
                contact = contact.getNext();
            }
            renderer.end();
        }
    }

    protected void renderBody(Body body) {
        Transform transform = body.getTransform();
        Fixture fixture = body.getFixtureList();
        while (fixture != null) {
            if (drawBodies) {
                drawShape(fixture, transform, getColorByBody(body));
                if (drawVelocities) {
                    Vec2 position = body.getPosition();
                    drawSegment(position, body.getLinearVelocity().add(position), VELOCITY_COLOR);
                }
            }

            if (drawAABBs) {
                drawAABB(fixture, transform);
            }
            fixture = fixture.getNext();
        }
    }

    private Color getColorByBody(Body body) {
        if (body.isActive() == false)
            return SHAPE_NOT_ACTIVE;
        else if (body.getType() == BodyType.STATIC)
            return SHAPE_STATIC;
        else if (body.getType() == BodyType.KINEMATIC)
            return SHAPE_KINEMATIC;
        else if (body.isAwake() == false)
            return SHAPE_NOT_AWAKE;
        else
            return SHAPE_AWAKE;
    }

    private void drawAABB(Fixture fixture, Transform transform) {
        if (fixture.getType() == ShapeType.CIRCLE) {

            CircleShape shape = (CircleShape) fixture.getShape();
            float radius = shape.getRadius();
            vertices[0].set(shape.m_p);
            Transform.mul(transform, vertices[0]);
            lower.set(vertices[0].x - radius, vertices[0].y - radius);
            upper.set(vertices[0].x + radius, vertices[0].y + radius);

            // define vertices in ccw fashion...
            vertices[0].set(lower.x, lower.y);
            vertices[1].set(upper.x, lower.y);
            vertices[2].set(upper.x, upper.y);
            vertices[3].set(lower.x, upper.y);

            drawSolidPolygon(vertices, 4, AABB_COLOR, true);
        } else if (fixture.getType() == ShapeType.POLYGON) {
            PolygonShape shape = (PolygonShape) fixture.getShape();
            int vertexCount = shape.getVertexCount();

            vertices[0] = shape.getVertex(0);
            lower.set(Transform.mul(transform, vertices[0]));
            upper.set(lower);
            for (int i = 1; i < vertexCount; i++) {
                vertices[i] = shape.getVertex(i);
                Transform.mul(transform, vertices[i]);
                lower.x = Math.min(lower.x, vertices[i].x);
                lower.y = Math.min(lower.y, vertices[i].y);
                upper.x = Math.max(upper.x, vertices[i].x);
                upper.y = Math.max(upper.y, vertices[i].y);
            }

            // define vertices in ccw fashion...
            vertices[0].set(lower.x, lower.y);
            vertices[1].set(upper.x, lower.y);
            vertices[2].set(upper.x, upper.y);
            vertices[3].set(lower.x, upper.y);

            drawSolidPolygon(vertices, 4, AABB_COLOR, true);
        }
    }

    private static Vec2 t;
    private static Vec2 axis = new Vec2(); //TODO: replace with jbox2d Rot

    private void drawShape(Fixture fixture, Transform transform, Color color) {
        if (fixture.getType() == ShapeType.CIRCLE) {
            CircleShape circle = (CircleShape) fixture.getShape();
            t = Transform.mul(transform, circle.m_p);
            drawSolidCircle(t, circle.getRadius(),
                    axis.set(transform.q.c, transform.q.s), color);
            return;
        }

        if (fixture.getType() == ShapeType.EDGE) {
            EdgeShape edge = (EdgeShape) fixture.getShape();
            vertices[0] = Transform.mul(transform, edge.m_vertex1);
            vertices[1] = Transform.mul(transform, edge.m_vertex2);
            drawSolidPolygon(vertices, 2, color, true);
            return;
        }

        if (fixture.getType() == ShapeType.POLYGON) {
            PolygonShape chain = (PolygonShape) fixture.getShape();
            int vertexCount = chain.getVertexCount();
            for (int i = 0; i < vertexCount; i++) {
                vertices[i] = Transform.mul(transform, chain.m_vertices[i]);
            }
            drawSolidPolygon(vertices, vertexCount, color, true);
            return;
        }

        if (fixture.getType() == ShapeType.CHAIN) {
            ChainShape chain = (ChainShape) fixture.getShape();
            int vertexCount = chain.getChildCount();
            for (int i = 0; i < vertexCount; i++) {
                vertices[i] = Transform.mul(transform, chain.m_vertices[i]);
            }
            drawSolidPolygon(vertices, vertexCount, color, false);
        }
    }

    private final Vec2 f = new Vec2();
    private final Vec2 v = new Vec2();
    private final Vec2 lv = new Vec2();

    private void drawSolidCircle(Vec2 center, float radius, Vec2 axis, Color color) {
        float angle = 0;
        float angleInc = 2 * (float) Math.PI / 20;
        renderer.setColor(color.r, color.g, color.b, color.a);
        for (int i = 0; i < 20; i++, angle += angleInc) {
            v.set((float) Math.cos(angle) * radius + center.x, (float) Math.sin(angle) * radius + center.y);
            if (i == 0) {
                lv.set(v);
                f.set(v);
                continue;
            }
            renderer.line(lv.x, lv.y, v.x, v.y);
            lv.set(v);
        }
        renderer.line(f.x, f.y, lv.x, lv.y);
        renderer.line(center.x, center.y, 0, center.x + axis.x * radius, center.y + axis.y * radius, 0);
    }

    private void drawSolidPolygon(Vec2[] vertices, int vertexCount, Color color, boolean closed) {
        renderer.setColor(color.r, color.g, color.b, color.a);
        lv.set(vertices[0]);
        f.set(vertices[0]);
        for (int i = 1; i < vertexCount; i++) {
            Vec2 v = vertices[i];
            renderer.line(lv.x, lv.y, v.x, v.y);
            lv.set(v);
        }
        if (closed) renderer.line(f.x, f.y, lv.x, lv.y);
    }

    private void drawJoint(Joint joint) {
        Body bodyA = joint.getBodyA();
        Body bodyB = joint.getBodyB();
        Transform xf1 = bodyA.getTransform();
        Transform xf2 = bodyB.getTransform();

        Vec2 x1 = xf1.p;
        Vec2 x2 = xf2.p;
        Vec2 p1 = new Vec2();
        joint.getAnchorA(p1);
        Vec2 p2 = new Vec2();
        joint.getAnchorB(p2);

        if (joint.getType() == JointType.DISTANCE) {
            drawSegment(p1, p2, JOINT_COLOR);
        } else if (joint.getType() == JointType.PULLEY) {
            PulleyJoint pulley = (PulleyJoint) joint;
            Vec2 s1 = pulley.getGroundAnchorA();
            Vec2 s2 = pulley.getGroundAnchorB();
            drawSegment(s1, p1, JOINT_COLOR);
            drawSegment(s2, p2, JOINT_COLOR);
            drawSegment(s1, s2, JOINT_COLOR);
        } else if (joint.getType() == JointType.MOUSE) {
            Vec2 j1 = new Vec2();
            Vec2 j2 = new Vec2();
            joint.getAnchorA(j1);
            joint.getAnchorB(j2);
            drawSegment(j1, j2, JOINT_COLOR);
        } else {
            drawSegment(x1, p1, JOINT_COLOR);
            drawSegment(p1, p2, JOINT_COLOR);
            drawSegment(x2, p2, JOINT_COLOR);
        }
    }

    private void drawSegment(Vec2 x1, Vec2 x2, Color color) {
        renderer.setColor(color);
        renderer.line(x1.x, x1.y, x2.x, x2.y);
    }

    private void drawContact(Contact contact) {
        WorldManifold worldManifold = new WorldManifold();
        contact.getWorldManifold(worldManifold);
        if (worldManifold.points.length == 0) return;
        Vec2 point = worldManifold.points[0];
        renderer.setColor(getColorByBody(contact.getFixtureA().getBody()));
        renderer.point(point.x, point.y, 0);
    }

    public boolean isDrawBodies() {
        return drawBodies;
    }

    public void setDrawBodies(boolean drawBodies) {
        this.drawBodies = drawBodies;
    }

    public boolean isDrawJoints() {
        return drawJoints;
    }

    public void setDrawJoints(boolean drawJoints) {
        this.drawJoints = drawJoints;
    }

    public boolean isDrawAABBs() {
        return drawAABBs;
    }

    public void setDrawAABBs(boolean drawAABBs) {
        this.drawAABBs = drawAABBs;
    }

    public boolean isDrawInactiveBodies() {
        return drawInactiveBodies;
    }

    public void setDrawInactiveBodies(boolean drawInactiveBodies) {
        this.drawInactiveBodies = drawInactiveBodies;
    }

    public boolean isDrawVelocities() {
        return drawVelocities;
    }

    public void setDrawVelocities(boolean drawVelocities) {
        this.drawVelocities = drawVelocities;
    }

    public boolean isDrawContacts() {
        return drawContacts;
    }

    public void setDrawContacts(boolean drawContacts) {
        this.drawContacts = drawContacts;
    }

    public static Vec2 getAxis() {
        return axis;
    }

    public static void setAxis(Vec2 axis) {
        Box2DDebugRenderer.axis = axis;
    }

    public void dispose() {
        renderer.dispose();
    }
}
