package aurelienribon.bodyeditor;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.jbox2d.collision.shapes.CircleShape;
import org.jbox2d.collision.shapes.PolygonShape;
import org.jbox2d.common.Vec2;
import org.jbox2d.dynamics.Body;
import org.jbox2d.dynamics.FixtureDef;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

/**
 * Loads the collision fixtures defined with the Physics Body Editor
 * application. You only need to give it a body and the corresponding fixture
 * name, and it will attach these fixtures to your body.
 *
 * @author Aurelien Ribon | http://www.aurelienribon.com
 *
 * Modified by xi.qian@gmail.com to use JBox2d on Android
 */
public class BodyEditorLoader {

    // Model
    private final Model model;

    // Reusable stuff
    private final List<Vec2> vectorPool = new ArrayList<Vec2>();
    private final PolygonShape polygonShape = new PolygonShape();
    private final CircleShape circleShape = new CircleShape();
    private final Vec2 vec = new Vec2();

    // -------------------------------------------------------------------------
    // Ctors
    // -------------------------------------------------------------------------

    public BodyEditorLoader(String str) {
        if (str == null)
            throw new NullPointerException("str is null");
        model = readJson(str);
        if(model == null)
            throw new NullPointerException("model is null");
    }

    // -------------------------------------------------------------------------
    // Public API
    // -------------------------------------------------------------------------

    /**
     * Creates and applies the fixtures defined in the editor. The name
     * parameter is used to retrieve the right fixture from the loaded file. <br/>
     * <br/>
     *
     * The body reference point (the red cross in the tool) is by default
     * located at the bottom left corner of the image. This reference point will
     * be put right over the BodyDef position point. Therefore, you should place
     * this reference point carefully to let you place your body in your world
     * easily with its BodyDef.position point. Note that to draw an image at the
     * position of your body, you will need to know this reference point (see
     * {@link #getOrigin(java.lang.String, float)}. <br/>
     * <br/>
     *
     * Also, saved shapes are normalized. As shown in the tool, the width of the
     * image is considered to be always 1 meter. Thus, you need to provide a
     * scale factor so the polygons get resized according to your needs (not
     * every body is 1 meter large in your game, I guess).
     *
     * @param body
     *            The Box2d body you want to attach the fixture to.
     * @param name
     *            The name of the fixture you want to load.
     * @param fd
     *            The fixture parameters to apply to the created body fixture.
     * @param scale
     *            The desired scale of the body. The default width is 1.
     */
    public void attachFixture(Body body, String name, FixtureDef fd, float scale) {
        RigidBodyModel rbModel = model.rigidBodies.get(name);
        if (rbModel == null)
            throw new RuntimeException("Name '" + name + "' was not found.");

        Vec2 origin = vec.set(rbModel.origin).mul(scale);

        for (int i = 0, n = rbModel.polygons.size(); i < n; i++) {
            PolygonModel polygon = rbModel.polygons.get(i);
            Vec2[] vertices = polygon.buffer;

            for (int ii = 0, nn = vertices.length; ii < nn; ii++) {
                vertices[ii] = newVec().set(polygon.vertices.get(ii))
                        .mul(scale);
                vertices[ii] = vertices[ii].sub(origin);
            }
            polygonShape.set(vertices, vertices.length);
            fd.shape = polygonShape;
            body.createFixture(fd);

            for (int ii = 0, nn = vertices.length; ii < nn; ii++) {
                free(vertices[ii]);
            }
        }

        for (int i = 0, n = rbModel.circles.size(); i < n; i++) {
            CircleModel circle = rbModel.circles.get(i);
            Vec2 center = newVec().set(circle.center).mul(scale);
            float radius = circle.radius * scale;

            circleShape.m_p.set(center);
            circleShape.setRadius(radius);
            fd.shape = circleShape;
            body.createFixture(fd);

            free(center);
        }
    }

    /**
     * Gets the image path attached to the given name.
     */
    public String getImagePath(String name) {
        RigidBodyModel rbModel = model.rigidBodies.get(name);
        if (rbModel == null)
            throw new RuntimeException("Name '" + name + "' was not found.");

        return rbModel.imagePath;
    }

    /**
     * Gets the origin point attached to the given name. Since the point is
     * normalized in [0,1] coordinates, it needs to be scaled to your body size.
     * Warning: this method returns the same Vector2 object each time, so copy
     * it if you need it for later use.
     */
    public Vec2 getOrigin(String name, float scale) {
        RigidBodyModel rbModel = model.rigidBodies.get(name);
        if (rbModel == null)
            throw new RuntimeException("Name '" + name + "' was not found.");

        return vec.set(rbModel.origin).mul(scale);
    }

    /**
     * <b>For advanced users only.</b> Lets you access the internal model of
     * this loader and modify it. Be aware that any modification is permanent
     * and that you should really know what you are doing.
     */
    public Model getInternalModel() {
        return model;
    }

    // -------------------------------------------------------------------------
    // Json Models
    // -------------------------------------------------------------------------

    public static class Model {
        public final Map<String, RigidBodyModel> rigidBodies = new HashMap<String, RigidBodyModel>();
    }

    public static class RigidBodyModel {
        public String name;
        public String imagePath;
        public final Vec2 origin = new Vec2();
        public final List<PolygonModel> polygons = new ArrayList<PolygonModel>();
        public final List<CircleModel> circles = new ArrayList<CircleModel>();
    }

    public static class PolygonModel {
        public final List<Vec2> vertices = new ArrayList<Vec2>();
        private Vec2[] buffer; // used to avoid allocation in attachFixture()
    }

    public static class CircleModel {
        public final Vec2 center = new Vec2();
        public float radius;
    }

    // -------------------------------------------------------------------------
    // Json reading process
    // -------------------------------------------------------------------------

    private Model readJson(String str) {
        Model m = new Model();
        // OrderedMap<String,?> rootElem = (OrderedMap<String,?>) new
        // JsonReader().parse(str);
        JSONObject rootElem;
        try {
            rootElem = (JSONObject) new JSONTokener(str).nextValue();

            // Array<?> bodiesElems = (Array<?>) rootElem.get("rigidBodies");
            JSONArray bodiesElems = rootElem.getJSONArray("rigidBodies");

            for (int i = 0; i < bodiesElems.length(); i++) {
                // OrderedMap<String,?> bodyElem = (OrderedMap<String,?>)
                // bodiesElems.get(i);
                JSONObject bodyElem = bodiesElems.getJSONObject(i);
                RigidBodyModel rbModel = readRigidBody(bodyElem);
                m.rigidBodies.put(rbModel.name, rbModel);
            }

            return m;
        } catch (JSONException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            return null;
        }
    }

    private RigidBodyModel readRigidBody(JSONObject bodyElem)
            throws JSONException {
        RigidBodyModel rbModel = new RigidBodyModel();
        rbModel.name = bodyElem.getString("name");
        rbModel.imagePath = bodyElem.getString("imagePath");

        JSONObject originElem = bodyElem.getJSONObject("origin");
        rbModel.origin.x = (float) originElem.getDouble("x");
        rbModel.origin.y = (float) originElem.getDouble("y");

        // polygons

        JSONArray polygonsElems = bodyElem.getJSONArray("polygons");

        for (int i = 0; i < polygonsElems.length(); i++) {
            PolygonModel polygon = new PolygonModel();
            rbModel.polygons.add(polygon);

            JSONArray verticesElems = polygonsElems.getJSONArray(i);
            for (int ii = 0; ii < verticesElems.length(); ii++) {
                JSONObject vertexElem = verticesElems.getJSONObject(ii);
                float x = (float) vertexElem.getDouble("x");
                float y = (float) vertexElem.getDouble("y");
                polygon.vertices.add(new Vec2(x, y));
            }

            polygon.buffer = new Vec2[polygon.vertices.size()];
        }

        // circles

        // Array<?> circlesElem = (Array<?>) bodyElem.get("circles");
        JSONArray circlesElems = bodyElem.getJSONArray("circles");

        for (int i = 0; i < circlesElems.length(); i++) {
            CircleModel circle = new CircleModel();
            rbModel.circles.add(circle);

            JSONObject circleElem = circlesElems.getJSONObject(i);
            circle.center.x = (float) circleElem.getDouble("cx");
            circle.center.y = (float) circleElem.getDouble("cy");
            circle.radius = (float) circleElem.getDouble("r");
        }

        return rbModel;
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    private Vec2 newVec() {
        return vectorPool.isEmpty() ? new Vec2() : vectorPool.remove(0);
    }

    private void free(Vec2 v) {
        vectorPool.add(v);
    }
}
