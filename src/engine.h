enum FaceType {
    VERTEX_ONLY,
    VERTEX_NORMAL,
    VERTEX_TEXTURE,
    VERTEX_ALL,
    VERTEX_ALL_ALPHA
};

struct Face {
    int vertices[3];
    int normals[3];
    int texture_coords[3];
};

struct Model {
    GLuint texture_id;
    FaceType face_type;

    vec3* vertices;
    vec3* normals;
    vec2* texture_coords;
    int num_faces;
    bool has_texture;

    // Note: currently only generated for the ground
    vec3* tangents;
    vec3* bitangents;
    bool has_tangents;
};

enum ObjectType {
    OBJ_CHARACTER,
    OBJ_GROUND
};

typedef struct {
    ObjectType type;

    int model_id; /* loaded_models[] */
    GLuint vao; /* vertex attrib obj */
    int shininess; /* 2-256 */

    float scale;
    vec3 pos;
    vec2 dir; /* TODO: do we need vec3 here? */
    float speed; /* maybe separate this field in another struct */

    float scale_tex_coords;
} Object;

/* Util */

struct File {
    vec3* vertices;
    int num_vertices;
    Face* faces;
    int num_faces;
    vec3* normals;
    int num_normals;
    vec2* texture_coords;
    int num_texture_coords;
};

enum LightType {
    DIRECTIONAL,
    POINTLIGHT,
    SPOTLIGHT,
};

struct Light {
    LightType type;
    vec3 pos;
    vec3 dir; // unused for POINTLIGHTs
    mat4 shadow_map_matrix; // unused for POINTLIGHTs
};

enum RenderPass {
    PASS_SHADOW_MAP,
    PASS_FINAL
};

enum CameraType {
    CAMERA_TARGETED,
    CAMERA_FREE,
};

struct Camera {
    CameraType type;
    vec3 pos;
    vec3 up;

    vec3 dir; // CAMERA_FREE
    vec3 *target; // CAMERA_TARGETED
};
