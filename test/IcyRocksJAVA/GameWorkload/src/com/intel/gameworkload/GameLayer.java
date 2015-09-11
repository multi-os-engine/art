/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.intel.gameworkload;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Stack;
import java.util.Vector;

import org.cocos2d.actions.instant.CCCallFuncN;
import org.cocos2d.actions.interval.CCFadeOut;
import org.cocos2d.actions.interval.CCMoveTo;
import org.cocos2d.actions.interval.CCSequence;
import org.cocos2d.actions.interval.CCSpawn;
import org.cocos2d.layers.CCLayer;
import org.cocos2d.config.ccMacros;
import org.cocos2d.events.CCTouchDispatcher;
import org.cocos2d.layers.*;
import org.cocos2d.nodes.*;
import org.cocos2d.types.*;
import org.jbox2d.callbacks.ContactImpulse;
import org.jbox2d.callbacks.ContactListener;
import org.jbox2d.common.Vec2;
import org.jbox2d.dynamics.*;
import org.jbox2d.collision.Manifold;
import org.jbox2d.collision.WorldManifold;
import org.jbox2d.collision.shapes.ChainShape;
import org.jbox2d.collision.shapes.CircleShape;
import org.jbox2d.collision.shapes.EdgeShape;
import org.jbox2d.dynamics.contacts.Contact;
import org.jbox2d.dynamics.joints.*;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Handler;
import android.util.Log;
import android.view.MotionEvent;
import aurelienribon.bodyeditor.BodyEditorLoader;

public class GameLayer extends CCColorLayer implements ContactListener {

    private static final float factor = 32.0f;
    private final float gravityFactor = 9.8f;

    private static final int sceneWidth = 1368;
    private static final int sceneHeight = 720;

    private static final int groundWidth = 1368;
    private static final int groundHeight = 20;

    private static final int rockWidth = 50;
    private static final int rockHeight = 100;
    private static final int nRocksPerRow = 20;

    private static final int snowWidth = 6;

    private static final int fanX = groundWidth/2;
    private static final int fanSize = 150;
    private static int fanY = fanSize/2+55;
    private static int fanSpeed = 10;

    private static final int MountainWidth = 600;
    private static final int MountainHeight = 384;

    private static final int ejectorHeight = MountainHeight;
    private static final int ejectorLength = 75;


    private static final int snowmanWidth = 90;
    private static final int snowmanHeight = 100;

    private static final int rockLabelWidth=130;
    private static final int rockLabelHeight = 50;
    private static final int rockNumberWidth = 30;


    private static final int leftWallTag = -2;
    private static final int particleTag = -1;
    private static final int ejectorbaseTag = 2;
    private static final int ejectorarmTag = 3;
    private static final int snowTag = 4;

    private static final int rockTag = 5;
    private static final int fanTag = 7;
    private static final int stoneTag = 8;
    private static final int disableStoneTag = 9;
    private static final int groundTag = 10;
    private static final int leftMTag = 11;
    private static final int rightMTag = 12;
    private static final int snowmanTag = 13;

    private boolean useCircleSnow = true;
    private boolean mScored = false;

    private static AssetManager mAm;
    private static MainActivity mActivity;
    private World world = null;
    private BodyEditorLoader loader = null;

    private int mNSnows = 0;
    private int mNRocks = 0;
    private int mSnowStep = 0;
    private int mRockStep = 0;

    private Stack<AutoScaleSprite> mScoreSprites=new Stack<AutoScaleSprite>();
    private Stack<AutoScaleSprite> mParticleSprites= new Stack<AutoScaleSprite>();
    private Stack<AutoScaleSprite> mStoneSprites=new Stack<AutoScaleSprite>();

    private AutoScaleSprite mRockLabel = null;
    private AutoScaleSprite mN1 = null;
    private AutoScaleSprite mN2 = null;
    private AutoScaleSprite mN3 = null;

    void init(int nRocks, int nSnows, int runtime)
    {
        mNSnows = nSnows;
        mNRocks = nRocks;
        CCDirector.sharedDirector().setnRocks(nRocks);
        CCDirector.sharedDirector().setnSnows(nSnows);

        CGSize winSize = CCDirector.sharedDirector().displaySize();
        Log.d("GameWorkload", "window "+winSize.width+"x"+winSize.height);
        AutoScaleSprite.setAutoScale(winSize.width/sceneWidth, winSize.height/sceneHeight);
        this.setIsTouchEnabled(true);

        bodiesToDel = new Vector<Body>();
        bodiesToReplace = new Vector<Body>();
        particleBodies = new LinkedList<ParticleRecord>();

        // create physics world
        Vec2 gravity = new Vec2(0.0f, -1 * gravityFactor);
        world = new World(gravity);

        // create box of the world
        BodyDef sideBodyDef = new BodyDef();
        sideBodyDef.position.set(0, 0);
        Body sideBody = world.createBody(sideBodyDef);

        ChainShape sideShape = new ChainShape();
        Vec2[] vertices = new Vec2[4];
        vertices[0] = new Vec2(0, groundHeight / factor);
        vertices[1] = new Vec2(sceneWidth / factor, groundHeight / factor);
        vertices[2] = new Vec2(sceneWidth / factor, sceneHeight / factor);
        vertices[3] = new Vec2(0, sceneHeight / factor);

        sideShape.createChain(vertices, 4);
        Fixture sideFixture = sideBody.createFixture(sideShape, 1);
        sideFixture.setFriction(0.01f);

        // draw ground. The sprite has some problem to deal with long
        AutoScaleSprite ground = AutoScaleSprite.sprite("images/ground.png");

        ground.setPosition(groundWidth / 2, groundHeight / 2);
        ground.setTag(groundTag);
        addChild(ground);
        sideBody.setUserData(ground);

        //create left wall
        BodyDef wallBodyDef = new BodyDef();
        wallBodyDef.position.set(0, 0);
        Body wallBody = world.createBody(sideBodyDef);

        EdgeShape wallShape = new EdgeShape();
        wallShape.set(new Vec2(0, sceneHeight/factor), new Vec2(0, groundHeight/factor));
        Fixture wallFixture = wallBody.createFixture(wallShape, 1);
        wallFixture.setFriction(0.01f);

        AutoScaleSprite fakeSprite = AutoScaleSprite.sprite("images/snow.png");
        fakeSprite.setTag(leftWallTag);
        wallBody.setUserData(fakeSprite);

        String jsonString = readFileToString("snowbodies.json");
        loader = new BodyEditorLoader(jsonString);

        Body leftM = createSpriteAndStaticBody("leftM", leftMTag, MountainWidth/2, MountainHeight/2, 2.0f);
        Body rightM = createSpriteAndStaticBody("rightM", rightMTag, sceneWidth-MountainWidth/2, MountainHeight/2, 2.0f);

        AutoScaleSprite logo = AutoScaleSprite.sprite("intel.png");
        logo.setPosition(sceneWidth-logo.getContentSize().width/2-1, logo.getContentSize().height/2);
        addChild(logo);

        if(runtime<4 && runtime>=0)
        {
            AutoScaleSprite runtimeLogo = AutoScaleSprite.sprite("runtime"+runtime+".png");
            runtimeLogo.setPosition(runtimeLogo.getContentSize().width/2+10, sceneHeight-runtimeLogo.getContentSize().height/2);
            addChild(runtimeLogo);
        }

        updateRockNumber();

        Body snowMan = createSpriteAndDynamicBody("snowman", snowmanTag, sceneWidth-snowmanWidth/2, MountainHeight+snowmanHeight/2, 50.0f, null);
        snowMan.setBullet(true);

        WeldJointDef snowmanJointDef = new WeldJointDef();
        snowmanJointDef.initialize(snowMan, rightM,
                snowMan.getPosition().sub(new Vec2(0, snowmanHeight/2/factor)));
        snowmanJointDef.collideConnected = true;
        world.createJoint(snowmanJointDef);

        // create snow
        for (int i = 0; i < mNSnows; i++) {
            this.createSnow((i * snowWidth + snowWidth / 2),
                    (int)(sceneHeight - 10 - sceneHeight/3 * ccMacros.CCRANDOM_0_1()), null);
        }

        for (int i = 0; i < mNRocks; i++) {
            String name = "rock" + (i % 5 + 1);
            createRock(name, rockWidth * (i%nRocksPerRow+1) + ejectorLength, sceneHeight - rockHeight * (i/nRocksPerRow+1), null);
        }

        // create Fan
        Body fanBody = this.createSpriteAndDynamicBody("fan", fanTag, fanX, fanY, 1.0f, null);
        fanBody.setBullet(true);

        // create fan joint
        RevoluteJointDef fanJointDef = new RevoluteJointDef();
        fanJointDef.initialize(sideBody, fanBody, fanBody.getPosition());
        fanJointDef.enableMotor = true;
        fanJointDef.motorSpeed = fanSpeed;
        fanJointDef.maxMotorTorque = 6000;
        RevoluteJoint fanJoint = (RevoluteJoint) world.createJoint(fanJointDef);


        // create ejector base and arm.
        Body baseBody = this.createSpriteAndStaticBody("ejectorbase", ejectorbaseTag,
                ejectorLength / 2, ejectorHeight, 1.0f);
        armBody = this.createSpriteAndDynamicBody("ejectorarm", ejectorarmTag,
                        ejectorLength, ejectorHeight + ejectorLength / 2, 1.0f, null);

        RevoluteJointDef armJointDef = new RevoluteJointDef();
        Vec2 anchor = new Vec2(ejectorLength / factor, ejectorHeight / factor);
        armJointDef.initialize(baseBody, armBody, anchor);
        armJointDef.enableMotor = true;
        armJointDef.motorSpeed = -15;
        armJointDef.maxMotorTorque = 700;
        armJointDef.enableLimit = true;
        armJointDef.lowerAngle = ccMacros.CC_DEGREES_TO_RADIANS(15);
        armJointDef.upperAngle = ccMacros.CC_DEGREES_TO_RADIANS(45);
        armJoint = (RevoluteJoint) world.createJoint(armJointDef);

        world.setContactListener(this);

        Log.w("GameWorkload", "before run scheduleupdate get class:" + this.getClass());
        this.scheduleUpdate();
    }

    protected GameLayer(ccColor4B color, int nRocks, int nSnows, int rockStep, int snowStep, int runtime, int mode) {
        // TODO Auto-generated constructor stub
        super(color);
        mRockStep = rockStep;
        mSnowStep = snowStep;
        init(nRocks, nSnows, runtime);
        benchmarkMode = (mode==1);
    }

    public static CCScene scene(MainActivity mainActivity, AssetManager am, int nRocks, int nSnows, int rockStep, int snowStep, int runtime, int mode)
    {
        CCScene scene = CCScene.node();
        mAm = am;
        mActivity = mainActivity;
        CCLayer layer = new GameLayer(ccColor4B.ccc4(122, 228, 255, 255), nRocks, nSnows, rockStep, snowStep, runtime, mode);
        scene.addChild(layer);
        return scene;
    }

    private class ParticleRecord
    {
        public Vector<Body> bodies;
        public float life;
        public ParticleRecord(Vector<Body> particles, float time)
        {
            bodies = particles;
            life = time;
        }
    }

    private LinkedList<ParticleRecord> particleBodies = null;

    /*
     * unit is in pixels
     * number means #particles for each side
     */
    private Vector<Body> createParticles(float x, float y, int width, int number, Vec2 initVelocity, String imagePath)
    {
        Vector<Body> bodies = new Vector<Body>();
        //calculate the rectangle for particles
        float length = width*number;
        float ix = x-length/2+width/2;
        float iy = y-length/2+width/2;
        float maxForce = 0.1f;
        Vec2 center = new Vec2(x/factor, y/factor);
        for(int i=0; i<number; i++)
        {
            for(int j=0; j<number; j++)
            {
                AutoScaleSprite sprite;
                if(mParticleSprites.isEmpty())
                {
                    sprite = AutoScaleSprite.sprite(imagePath);
                    if(sprite.getContentSize().width!=width)
                    {
                        sprite.setScale(width/sprite.getContentSize().width);
                    }
                    addChild(sprite);
                }else
                {
                    sprite = mParticleSprites.pop();
                    sprite.setVisible(true);
                }
                sprite.setPosition(ix, iy);
                sprite.setTag(particleTag);

                BodyDef bodyDef = new BodyDef();
                bodyDef.position.set(sprite.getPosition().x / factor,
                        sprite.getPosition().y / factor);
                bodyDef.type = BodyType.DYNAMIC;
                bodyDef.userData = sprite;

                FixtureDef fixtureDef = new FixtureDef();
                fixtureDef.density = 0.5f;
                fixtureDef.friction = 0.7f;
                fixtureDef.restitution = 0.7f;

                Body body = world.createBody(bodyDef);
                CircleShape shape = new CircleShape();
                shape.setRadius(width / (factor * 2));
                fixtureDef.shape = shape;
                body.createFixture(fixtureDef);

                body.setLinearDamping(0.8f);
                Vec2 force = body.getPosition().sub(center);
                float distance = force.normalize();
                body.setLinearVelocity(initVelocity);
                body.applyLinearImpulse(force.mul(maxForce), body.getPosition());
                bodies.add(body);
                ix = ix+width;
            }
            ix = x-length/2;
            iy = iy+width;
        }
        return bodies;
    }

    private void createScoreAnimation(float seconds)
    {
        AutoScaleSprite scoreSprite;
        if(mScoreSprites.isEmpty())
        {
            scoreSprite = AutoScaleSprite.sprite("images/score.png");
            scoreSprite.setPosition(sceneWidth/2, 0-scoreSprite.getContentSize().height/2);
            addChild(scoreSprite);
        }else
        {
            scoreSprite = mScoreSprites.pop();
            scoreSprite.setOpacity(255);
            scoreSprite.setPosition(sceneWidth/2, 0-scoreSprite.getContentSize().height/2);
            scoreSprite.setVisible(true);
        }

        CCMoveTo actionMove = CCMoveTo.action(seconds, CGPoint.ccp(sceneWidth/2, sceneHeight+scoreSprite.getContentSize().height/2));
        CCFadeOut actionFadeOut = CCFadeOut.action(seconds);
        CCSpawn spawnActions = CCSpawn.actions(actionMove, actionFadeOut);
        CCCallFuncN actionMoveDone = CCCallFuncN.action(this, "scoreFinished");
        CCSequence actions = CCSequence.actions(spawnActions, actionMoveDone);
        scoreSprite.runAction(actions);
    }

    public void scoreFinished(Object sender)
    {
        AutoScaleSprite sprite = (AutoScaleSprite)sender;
        sprite.setVisible(false);
        mScoreSprites.push(sprite);
    }

    private final float LogInterval = 5f;
    private float timeToLog = LogInterval;
    private boolean logEnabled = false;

    private final float fireInterval = 3f;
    private float timeToFire = fireInterval;

    private final float windInterval = 0.1f;
    private float timeToWind = windInterval;
    private Vector<Body> bodiesToDel;
    private Vector<Body> bodiesToReplace;

    private final float benchmarkInterval = 20f;
    private final int benchmarkRockStep = 20;
    private final int benchmarkSnowStep = 50;
    private final int benchmarkMaxSteps = 6;
    private int benchmarkSteps = 0;
    private boolean benchmarkMode = false;
    private float benchmarkLastUpdateTime = 0;
    private long benchmarkLastContacts = 0;
    private float benchmarkTotalUpdateTime = 0;
    private long benchmarkTotalContacts=0;
    private float benchmarkTotalFrames = 0;
    private double benchmarkAFPS = 1;
    private double benchmarkAUPF = 1;
    private double benchmarkAAPS = 1;
    private double benchmarkJPS = 0;

    private String benchmarkdetails = "";

    private void outputresult(String s)
    {
        benchmarkdetails += s+"\n";
        Log.d("GameWorkload", s);
    }

    private void appenddetails(String s)
    {
        benchmarkdetails += s+"\n";
    }

    private void printdetaiils(float afps, double aut, double aaps, float jps, float acps)
    {
        appenddetails("Calculating avg fps @"+afps);
        appenddetails("Calculating Average update time @"+aut);
        if(aaps>0)
            appenddetails("Calculating Average animation/second @"+aaps);
        appenddetails("Calculating Average JPS @"+jps);
        appenddetails("Calculating Average Contacts/frame @"+acps);

        String s = String.format("%d %d %.4f %.4f %.4f %.4f %.4f", mNRocks, mNSnows, afps, aut, aaps, jps, acps);
        Log.d("GameWorkload", s);
        benchmarkAFPS *= afps;
        benchmarkAUPF *= aut;
        if(aaps>0)
            benchmarkAAPS *= aaps;
        benchmarkJPS += jps;
    }

    private class MyTask implements Runnable{
        String mResult;
        String mDetails;
        public MyTask(String result, String details)
        {
            mResult = result;
            mDetails = details;
        }
        @Override
        public void run() {
            Intent resultData = new Intent();
            resultData.putExtra("benchmarkdetails", mDetails);
            resultData.putExtra("benchmarkresult", mResult);
            mActivity.setResult(Activity.RESULT_OK, resultData);
        }
    }

    String [] resultDescriptions = {"Animation/s", "FPS", "UpdateTimeInSec/frame", "JANK/s"};
    double [] resultValues = {0, 0, 0, 0};
    int [] precision = {0, 1, 4, 4};
    private String getBenchmarkResult()
    {
        if(benchmarkSteps>1)
            resultValues[0] = Math.pow(benchmarkAAPS, 1/(double)(benchmarkSteps-1));
        resultValues[1] = Math.pow(benchmarkAFPS, 1/(double)benchmarkSteps);
        resultValues[2] = Math.pow(benchmarkAUPF, 1/(double)benchmarkSteps);
        resultValues[3] = benchmarkJPS/(float)benchmarkSteps;

        JSONObject result = new JSONObject();
        try {
            for (int i = 0; i < 4; i++) {
                JSONObject content = new JSONObject();
                content.put("description", resultDescriptions[i]);
                content.put("value", resultValues[i]);
                content.put("precision", precision[i]);
                result.put(Integer.toString(i), content);
            }
        } catch (JSONException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }


        String benchmarkresult = result.toString();
        return benchmarkresult;
    }

    private void updateActivityResult()
    {
        Handler handler = new Handler(mActivity.getMainLooper());
        handler.post(new MyTask(getBenchmarkResult(), benchmarkdetails));
    }

    private void exitActivity()
    {
        Handler handler = new Handler(mActivity.getMainLooper());
        handler.post(new Runnable() {
            public void run() {
                mActivity.finish();
            }
        });
    }

    public void update(float dt) {

        if(benchmarkMode) {
            if(benchmarkTotalFrames==1 && benchmarkSteps==0)
            {
                //print out first frame hint
                appenddetails("Running settings @"+mNRocks+" rocks @"+mNSnows+" snows");
            }
            float totalTime = CCDirector.sharedDirector().gettotalTime();
            if(totalTime>0)
            {
                benchmarkTotalUpdateTime += benchmarkLastUpdateTime;
                benchmarkTotalFrames++;
                benchmarkTotalContacts += benchmarkLastContacts;
            }
            if (totalTime >= benchmarkInterval) {
                this.benchmarkSteps++;
                appenddetails("Ended settings @"+mNRocks+" rocks @"+mNSnows+" snows");
                CCDirector d = CCDirector.sharedDirector();
                printdetaiils(d.getAFPS(), d.getAUTPF(), d.getAAPS(), d.getJPS(), benchmarkTotalContacts/(float)benchmarkTotalFrames);

                updateActivityResult();
                if (benchmarkSteps >= benchmarkMaxSteps) {
                    outputresult("Benchmark ended");
                    //outputresult(getBenchmarkResult());
                    updateActivityResult();
                    CCDirector.sharedDirector().end();
                    exitActivity();

                } else {
                    benchmarkTotalUpdateTime = 0;
                    benchmarkTotalFrames = 0;
                    benchmarkTotalContacts = 0;
                    appenddetails("Running settings @"+mNRocks+" rocks @"+mNSnows+" snows");
                    addObjects(benchmarkRockStep, benchmarkSnowStep);
                }
            }
        }

        long t0 = System.currentTimeMillis();
        world.step(1.0f/60, 10, 10);
        int nContacts = world.getContactCount();
        benchmarkLastContacts= nContacts;
        long t1 = System.currentTimeMillis();
        if(mScored)
        {
            createScoreAnimation(2);
            mScored = false;
        }

        timeToLog -= dt;
        timeToFire -= dt;
        timeToWind -= dt;

        if (timeToFire < 0) {
            fire();
            timeToFire = fireInterval;
        }

        if (timeToLog < 0 && logEnabled)
        {
            float AFPS = CCDirector.sharedDirector().getAFPS();
            float Time = CCDirector.sharedDirector().gettotalTime();
            Log.d("GameWorkload", "TotalTime="+Time+" AFPS="+AFPS+" Rock="+mNRocks+" Snow="+mNSnows+" Sprites="+this.getChildren().size()+" particleSprites="+mParticleSprites.size());
            timeToLog = LogInterval;
        }

        float xForce = 0;
        if(timeToWind<0)
            xForce = ccMacros.CCRANDOM_0_1();

        Body body = world.getBodyList();
        bodiesToDel.clear();
        bodiesToReplace.clear();

        for(Iterator<ParticleRecord> i = particleBodies.iterator(); i.hasNext();)
        {
            ParticleRecord r = i.next();
            r.life -= dt;
            if(r.life<0)
            {
                for(Body b : r.bodies)
                {
                    //this.removeChild((CCNode) b.getUserData(), true);
                    AutoScaleSprite sprite = (AutoScaleSprite) b.getUserData();
                    sprite.setVisible(false);
                    mParticleSprites.push(sprite);
                    world.destroyBody(b);
                }
                r.bodies.clear();
                i.remove();
            }
            else
            {
                for(Body b : r.bodies)
                {
                    AutoScaleSprite s = (AutoScaleSprite) b.getUserData();
                    s.setOpacity(Math.round(255*r.life));
                }
            }
        }

        int nBodiesOut=0;

        // check stoneJoint
        if (stoneJoint != null) {
            if (stoneJoint.getBodyA().getLinearVelocity().x > 0) {
                if (armJoint.getJointAngle() <= ccMacros
                        .CC_DEGREES_TO_RADIANS(20)) {
                    world.destroyJoint(stoneJoint);
                    stoneJoint = null;
                }
            }
        }

        while (body != null) {

            if(body.getType()==BodyType.STATIC)
            {
                body = body.getNext();
                continue;
            }
            AutoScaleSprite sprite = (AutoScaleSprite) body.m_userData;

            if (sprite.getTag() == disableStoneTag) {
                // apply exploiting
                Vec2 initVelocity = body.getLinearVelocity().mul(0.1f);
                particleBodies.add(new ParticleRecord(this.createParticles(sprite.getPosition().x, sprite.getPosition().y, 6, 4, initVelocity, "images/particle.png"), 1));
                bodiesToDel.add(body);

                if (stoneJoint != null && stoneJoint.getBodyA() == body) {
                    // remove joint
                    world.destroyJoint(stoneJoint);
                    stoneJoint = null;
                }
            } else if (sprite.getTag() != groundTag && sprite.getTag() != leftWallTag) {
                sprite.setPosition(body.getPosition().x * factor,
                        body.getPosition().y * factor);
                sprite.setRotation(-1
                        * ccMacros.CC_RADIANS_TO_DEGREES(body.getAngle()));
            }

            // apply wind force

            if(timeToWind<0 && sprite.getTag()==snowTag)
            {
                if(body.getPosition().x*factor<ejectorLength*2)
                {
                    body.applyLinearImpulse(new Vec2(xForce*body.getMass(), 0), body.getPosition());

                }else if(body.getPosition().x*factor>sceneWidth-snowmanWidth*2)
                {
                    body.applyLinearImpulse(new Vec2(-1*xForce*body.getMass(), 0), body.getPosition());
                }
            }

            float bx = body.getPosition().x*factor;
            float by = body.getPosition().y*factor;
            if((bx<0 || bx>sceneWidth|| by<groundHeight-2 || by>sceneHeight)&&sprite.getTag()!=particleTag)
            {

                //Log.d("GameWorkload", "Body out of box, tag "+sprite.getTag()+ " x "+body.getPosition().x+ " y "+body.getPosition().y);
                bodiesToReplace.add(body);
            }

            body = body.getNext();
        }

        for (int i = 0; i < bodiesToDel.size(); i++) {
            Body delBody = bodiesToDel.get(i);
            AutoScaleSprite sprite = (AutoScaleSprite) delBody.getUserData();
            sprite.setVisible(false);
            if(sprite.getTag()==this.particleTag)
            {
                mParticleSprites.push(sprite);
            }else if(sprite.getTag()==this.disableStoneTag)
            {
                mStoneSprites.push(sprite);
            }
            //this.removeChild(sprite, true);
            world.destroyBody(delBody);
        }

        int x = sceneWidth/2;
        for(int i=0; i<bodiesToReplace.size(); i++)
        {
            Body replaceBody = bodiesToReplace.get(i);
            AutoScaleSprite sprite = (AutoScaleSprite) replaceBody.getUserData();
            if(sprite.getTag()==rockTag)
            {
                this.createRock((String) sprite.getUserData(), x, (int)(sceneHeight-sprite.getContentSize().height), sprite);
            }else if(sprite.getTag()==snowTag)
            {
                this.createSnow(x, (int)(sceneHeight-sprite.getContentSize().height), sprite);
            }
            x = x + (int)sprite.getContentSize().width;
            world.destroyBody(replaceBody);
            //this.removeChild(sprite, true);
        }
        bodiesToReplace.clear();

        if (timeToWind < 0)
            timeToWind = windInterval;
        CCDirector.sharedDirector().setObjNumber(world.getBodyCount());
        bodiesToDel.clear();
        long t2 = System.currentTimeMillis();
        benchmarkLastUpdateTime = (float)(t2 - t0)/1000.f;
        //Log.d("GameWorkload", "contacts="+nContacts+" lastTotalTime "+dt*1000+" updateTime="+(t2-t0)+" stepTime="+(t1-t0)+" otherTime="+(t2-t1)+"benchmarkUpdateTime "+this.benchmarkLastUpdateTime);
        CCDirector.sharedDirector().setUpdateTime(benchmarkLastUpdateTime);
    }

    private void addObjects(int nRocks, int nSnows)
    {
        mNSnows += nSnows;
        mNRocks += nRocks;
        for (int i = 0; i < nSnows; i++) {
            this.createSnow((i * snowWidth + snowWidth / 2),(int)(sceneHeight - 10 - sceneHeight/3 * ccMacros.CCRANDOM_0_1()), null);
        }
        for( int i=0; i < nRocks; i++)
        {
            String name = "rock"+(i%5+1);
            createRock(name, rockWidth * (i%nRocksPerRow+1) + ejectorLength, sceneHeight - rockHeight * (i/nRocksPerRow+1), null);
        }
        CCDirector.sharedDirector().resetAFPS();
        CCDirector.sharedDirector().setnRocks(mNRocks);
        CCDirector.sharedDirector().setnSnows(mNSnows);
        updateRockNumber();
    }

    private Body createRock(String name, int x, int y, AutoScaleSprite oldSprite)
    {
        return createSpriteAndBody(name, rockTag, x, y, 0.5f, 0.1f, 0.8f, BodyType.DYNAMIC, oldSprite, false);
    }

    private Body createSnow(int x, int y, AutoScaleSprite oldSprite)
    {
        Body body = createSpriteAndBody("snow", snowTag, x, y, 0.5f, 0.7f, 0.001f, BodyType.DYNAMIC, oldSprite, this.useCircleSnow);
        body.setGravityScale(0.05f);
        body.setLinearDamping(1);
        return body;
    }

    private Body createSpriteAndDynamicBody(String name, int tag, int x, int y, float density, AutoScaleSprite oldSprite) {
        return createSpriteAndBody(name, tag, x, y, density, 0.2f, 0.8f, BodyType.DYNAMIC, oldSprite, false);
    }


    private Body createSpriteAndStaticBody(String name, int tag, int x, int y,    float density) {
        return createSpriteAndBody(name, tag, x, y, density, 0.2f, 0.8f, BodyType.STATIC, null, false);
    }

    private Body createSpriteAndBody(String name, int tag, int x, int y, float density, float friction, float restitution,
            BodyType type, AutoScaleSprite oldSprite, boolean useCircle) {

        AutoScaleSprite sprite;
        if(oldSprite!=null)
        {
            sprite = oldSprite;
            sprite.setVisible(true);
        }
        else
        {
            sprite = AutoScaleSprite.sprite(loader.getImagePath(name));
            addChild(sprite);
        }
        sprite.setPosition(x, y);
        sprite.setTag(tag);

        BodyDef bodyDef = new BodyDef();
        bodyDef.position.set(sprite.getPosition().x / factor,
                sprite.getPosition().y / factor);
        bodyDef.type = type;
        bodyDef.userData = sprite;

        FixtureDef fixtureDef = new FixtureDef();
        fixtureDef.density = density;
        fixtureDef.friction = friction;
        fixtureDef.restitution = restitution;

        Body body = world.createBody(bodyDef);

        if (useCircle) {
            CircleShape shape = new CircleShape();
            shape.setRadius(snowWidth / (factor * 2));
            fixtureDef.shape = shape;
            body.createFixture(fixtureDef);
        }else
        {
            loader.attachFixture(body, name, fixtureDef, sprite.getContentSize().width / factor);
        }
        // 4. Create the body fixture automatically by using the loader.
        sprite.setUserData(name);
        return body;
    }

    private Body armBody = null;
    //private Body baseBody = null;
    private WeldJoint stoneJoint = null;
    private RevoluteJoint armJoint = null;


    public void fire() {
        // create magic stone
        if (stoneJoint == null) {
            Body stoneBody;
            if(mStoneSprites.isEmpty())
            {
                stoneBody = this.createSpriteAndDynamicBody("stone", stoneTag,
                    ejectorLength, ejectorHeight + ejectorLength, 1.5f, null);
            }else
            {
                stoneBody = this.createSpriteAndDynamicBody("stone", stoneTag,
                        ejectorLength, ejectorHeight + ejectorLength, 1.5f, mStoneSprites.pop());
            }
            stoneBody.setBullet(true);
            WeldJointDef stoneJointDef = new WeldJointDef();
            stoneJointDef.initialize(stoneBody, armBody,
                    stoneBody.getPosition());
            stoneJointDef.collideConnected = false;
            stoneJoint = (WeldJoint) world.createJoint(stoneJointDef);

            // apply force to arm
            armBody.applyAngularImpulse(1000);
        }
    }

    private String readFileToString(String fname) {
        try {
            StringBuilder builder = new StringBuilder(1024);
            BufferedReader br = new BufferedReader(new InputStreamReader(
                    mAm.open(fname)));
            String line;
            while ((line = br.readLine()) != null) {
                builder.append(line).append("\n");
            }
            br.close();
            return builder.toString();
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
    }

    @Override
    public boolean ccTouchesBegan(MotionEvent event) {
        return CCTouchDispatcher.kEventHandled;
    }

    @Override
    public boolean ccTouchesEnded(MotionEvent event) {
        Log.d("GameWorkload", "touch end");
        if(!benchmarkMode)
        {
            addObjects(mRockStep, mSnowStep);
        }
        return super.ccTouchesEnded(event);

    }

    @Override
    public boolean ccTouchesMoved(MotionEvent event) {
        return super.ccTouchesMoved(event);
    }

    @Override
    public boolean ccTouchesCancelled(MotionEvent event) {
        return super.ccTouchesCancelled(event);
    }

    @Override
    public void beginContact(Contact contact) {
        // TODO Auto-generated method stub
    }

    @Override
    public void endContact(Contact contact) {
        // TODO Auto-generated method stub

    }

    @Override
    public void preSolve(Contact contact, Manifold oldManifold) {
        // TODO Auto-generated method stub
        // get obj of contact
    }

    @Override
    public void postSolve(Contact contact, ContactImpulse impulse) {
        AutoScaleSprite spriteA = (AutoScaleSprite) contact.getFixtureA().getBody()
                .getUserData();
        AutoScaleSprite spriteB = (AutoScaleSprite) contact.getFixtureB().getBody()
                .getUserData();

        if (spriteA.getTag() <= snowTag || spriteB.getTag() <= snowTag) {
            return;
        }

        if(spriteB.getTag() == stoneTag)
        {
            AutoScaleSprite tmp = spriteA;
            spriteA = spriteB;
            spriteB = tmp;
        }

        if (spriteA.getTag() == stoneTag) {
//            Log.d("GameWorkload", "special hit tagA " + spriteA.getTag() + " tagB " + spriteB.getTag());
            WorldManifold worldManifold = new WorldManifold();
            contact.getWorldManifold(worldManifold);
            spriteA.setTag(disableStoneTag);
            spriteA.setPosition(worldManifold.points[0].x * factor,
                    worldManifold.points[0].y * factor);

            if(spriteB.getTag() == snowmanTag)
            {
                mScored = true;
            }
        }
    }

    private void updateRockNumber()
    {
        if(mRockLabel==null)
        {
            mRockLabel = AutoScaleSprite.sprite("rocklabel.png");
            this.addChild(mRockLabel);
        }
        if(mNRocks>999)
            return;
        //calculate numbers for 1x, 10x, 100x
        int d1 = mNRocks%10;
        int d10 = mNRocks%100/10;
        int d100 = mNRocks/100;

        float x = sceneWidth - 3*rockNumberWidth-rockLabelWidth/2;
        float y = sceneHeight - rockLabelHeight/2;
        mRockLabel.setPosition(x, y);

        x = sceneWidth - 2.5f*rockNumberWidth;
        if(d100>0)
        {
            AutoScaleSprite n = AutoScaleSprite.sprite(d100+".png");
            n.setPosition(x, y);
            x += rockNumberWidth;
            if(mN1 != null)
                this.removeChild(mN1, true);
            mN1 = n;
            this.addChild(n);
        }
        if(d100>0 || d10>0)
        {
            AutoScaleSprite n = AutoScaleSprite.sprite(d10+".png");
            n.setPosition(x, y);
            x += rockNumberWidth;
            if(mN2 != null)
                this.removeChild(mN2, true);
            mN2 = n;
            this.addChild(n);
        }
        {
            AutoScaleSprite n = AutoScaleSprite.sprite(d1+".png");
            n.setPosition(x, y);
            x += rockNumberWidth;
            if(mN3 != null)
                this.removeChild(mN3, true);
            mN3 = n;
            this.addChild(n);
        }
    }

}
