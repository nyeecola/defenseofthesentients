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
};

typedef struct {
    int model_id; /* loaded_models[] */
    mat4 mat; /* model mat */
    GLuint vao; /* vertex attrib obj */
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
