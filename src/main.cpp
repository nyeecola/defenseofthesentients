#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <cmath>

#define CGLM_ALL_UNALIGNED
#include <cglm/cglm.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "defines.h"
#include "engine.h"

#include "globals.cpp"
#include "model.cpp"
//#include "model2.cpp"

#define MIN2(a,b) ((a < b) ? (a) : (b))
#define MAX2(a,b) ((a > b) ? (a) : (b))

#define POLL_GL_ERROR poll_gl_error(__FILE__, __LINE__)

void poll_gl_error(const char *file, long long line) {
    int err = glGetError();
    if (err) {
        printf("%s: line %lld: error %d, ", file, line, err);
        exit(1);
    }
}

static void error_callback(int error, const char* description) {
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        grid_enabled = !grid_enabled;
    }
}

// TODO: caller must free buffer
char* load_file(char const* path) {
    char* buffer = 0;
    long length;
    FILE* f = fopen(path, "rb");

    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (char*)malloc((length + 1) * sizeof(char));

        if (buffer) {
            fread(buffer, sizeof(char), length, f);
        }
        else {
            // TODO: clean up all aborts
            abort();
        }
        fclose(f);
    }

    buffer[length] = '\0';

    return buffer;
}

GLuint compile_shader(GLuint stage, char *filename) {
    // TODO: free
    char* vertex_shader_text = load_file(filename);

    GLuint shader = glCreateShader(stage);
    glShaderSource(shader, 1, (const char* const*)&vertex_shader_text, NULL);
    glCompileShader(shader);

    // TODO: fix the size
    GLchar shader_info_buffer[200];
    GLint shader_info_len;
    glGetShaderInfoLog(shader, 200, &shader_info_len, shader_info_buffer);
    if (shader_info_len) printf("Shader stage %d error: %s\n", stage, shader_info_buffer);
    
    return shader;
}

GLuint create_program(GLuint vert, GLuint frag) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    // TODO: fix the size
    GLchar shader_info_buffer[200];
    GLint shader_info_len;
    glGetProgramInfoLog(program, 200, &shader_info_len, shader_info_buffer);
    if (shader_info_len) printf("Shader linking error: %s\n", shader_info_buffer);

    return program;
}

GLuint create_shadow_map_program() {
    GLuint vert = compile_shader(GL_VERTEX_SHADER, "shaders/shadow_vert.glsl");
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, "shaders/shadow_frag.glsl");
    return create_program(vert, frag);
}

// TODO: refactor
void blit_texture(GLuint width, GLuint height, GLuint texture) {
    static bool initialized = false;
    static GLuint program;
    static GLuint VAO;
    static GLuint tex;

    if (!initialized) {
        GLuint vert = compile_shader(GL_VERTEX_SHADER, "shaders/blit_vert.glsl");
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, "shaders/blit_frag.glsl");
        program = create_program(vert, frag);
        initialized = true;
    
        GLuint VBO;
        glGenBuffers(1, &VBO);

        GLfloat vertices[] = {
            0.0f, 0.0f, 0.0f,      0.0f, 1.0f,
            0.0f, 1.0f, 0.0f,      0.0f, 0.0f,
            1.0f, 1.0f, 0.0f,      1.0f, 0.0f,
            0.0f, 0.0f, 0.0f,      0.0f, 1.0f,
            1.0f, 1.0f, 0.0f,      1.0f, 0.0f,
            1.0f, 0.0f, 0.0f,      1.0f, 1.0f,
        };
        POLL_GL_ERROR;
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 5, (void*) 0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 5, (void*)(3 * sizeof(GLfloat)));
        glBindVertexArray(0);
        POLL_GL_ERROR;
    }

    // TODO: fix this blit texture thing, something is very broken
    //       maybe this comment is outdated
    glUseProgram(program);
    POLL_GL_ERROR;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    POLL_GL_ERROR;
    glUniform1i(glGetUniformLocation(program, "tex"), 0);
    POLL_GL_ERROR;
    glDisable(GL_CULL_FACE);
    glClearColor(1.0, 0.0, 0.0, 1);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    POLL_GL_ERROR;
}

void screen_to_world_space_ray(vec3 camera_pos, float x, float y,
                               mat4 proj_mat, mat4 view_mat,
                               vec3 out_ray_origin, vec3 out_ray_dir)
{
	vec4 ray_clip = { x, y, -1, 1 };
	mat4 proj_mat_inv;
	glm_mat4_inv(proj_mat, proj_mat_inv);
	glm_mat4_mulv(proj_mat_inv, ray_clip, ray_clip);

	vec4 ray_eye = { ray_clip[0], ray_clip[1], -1, 0 };
	mat4 view_mat_inv;
	glm_mat4_inv(view_mat, view_mat_inv);
	glm_mat4_mulv(view_mat_inv, ray_eye, ray_eye);

    // we're in world space now
    glm_vec3_copy(ray_eye, out_ray_dir);
	glm_vec3_normalize(out_ray_dir);

    glm_vec3_copy(camera_pos, out_ray_origin);
}

void ray_plane_intersection(vec3 ray_origin, vec3 ray_dir,
                            vec3 plane_normal, float plane_offset,
                            vec3 out_point)
{
	// Ray vs Plane
	// Plane equation: dot(N, P) + d = 0
	// Ray equation: P = O + t * D
	//
	// Intersection: dot(N, O + t * D) + d = 0
	// dot(N, O + t * D) + d = 0
	// dot(N, O) + dot(N, t * D) + d = 0
	// dot(N, O) + dot(N, D) * t + d = 0
	// dot(N, D) * t = - (dot(N, O) + d)
	// t = - (dot(N, O) + d) / dot(N, D)
    float denominator = glm_vec3_dot(plane_normal, ray_dir);
    if (abs(denominator) < 0.0001f) {
        vec3 zero = GLM_VEC3_ZERO_INIT;
        glm_vec3_copy(zero, out_point);
        return;
    }
        
    float t = -(glm_vec3_dot(plane_normal, ray_origin) - plane_offset) /
				denominator;
    glm_vec3_scale(ray_dir, t, ray_dir);
    glm_vec3_add(ray_origin, ray_dir, out_point);
}

// TODO: put all this state in a struct
void render_scene(float width, float height, float mouse_x, float mouse_y,
                  vec3 camera_pos, Light *light, mat4 proj_mat, mat4 view_mat,
                  Object **scene_geometry, int num_scene_geom,
                  GLuint program, RenderPass pass)
{
    glUseProgram(program);

    switch (pass) {
    case PASS_FINAL:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case PASS_SHADOW_MAP:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    default:
        assert(false && "UNKNOWN RENDER PASS!");
        break;
    }

    mat4 view_proj;
    glm_mat4_identity(view_proj);
    glm_mat4_mul(proj_mat, view_mat, view_proj);

    glEnable(GL_DEPTH_TEST);

    glViewport(0, 0, width, height);
    glClearColor(0.0, 0.0, 0.0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    switch (pass) {
    case PASS_FINAL:
        // TODO: refactor this to make it maintainable with many textures
        glUniform1i(glGetUniformLocation(program, "shadowMap"), 0);
        glUniform1i(glGetUniformLocation(program, "ditherPattern"), 1);
        glUniform1i(glGetUniformLocation(program, "textureA"), 2);
        glUniform1i(glGetUniformLocation(program, "normalMap"), 3);
        glUniformMatrix4fv(glGetUniformLocation(program, "shadow_map_matrix"), 1, GL_FALSE, (const GLfloat*)light->shadow_map_matrix);
		glUniform3fv(glGetUniformLocation(program, "cameraPos"), 1, camera_pos);
        break;
    case PASS_SHADOW_MAP:
		glUniform3fv(glGetUniformLocation(program, "cameraPos"), 1, light->pos);
        glm_mat4_copy(view_proj, light->shadow_map_matrix);
        break;
    default:
        assert(false && "UNKNOWN RENDER PASS!");
        break;
    }

    glUniform3fv(glGetUniformLocation(program, "lightPos"), 1, light->pos);
    glUniform1f(glGetUniformLocation(program, "farPlane"), FAR_PLANE);
    glUniformMatrix4fv(glGetUniformLocation(program, "view_proj"), 1, GL_FALSE, (const GLfloat*)view_proj);

    for (int i = 0; i < num_scene_geom; i++) {
        Object *obj = scene_geometry[i];
        if (obj->type == OBJ_GROUND) {
			if (pass == PASS_FINAL) { // we don't care about the grid when doing shadow mapping
				glUniform1i(glGetUniformLocation(program, "gridEnabled"), grid_enabled);
                // TODO: move this out, perhaps use a "RenderState" struct to pass 
                //       PASS-specific arguments
                vec3 ray_origin, ray_dir;
                screen_to_world_space_ray(camera_pos, mouse_x, mouse_y,
                                          proj_mat, view_mat,
                                          ray_origin, ray_dir);
                vec3 plane_normal = { 0.0, 1.0, 0.0 };
                vec3 target_pos;
                ray_plane_intersection(ray_origin, ray_dir, plane_normal, 0.0f, target_pos);
				glUniform3fv(glGetUniformLocation(program, "cursorPos"), 1, target_pos);
			}
            //draw_model_force_rgb(program, *obj, 0.7, 0.4, 0.08);
            draw_model(program, *obj, pass);
			glUniform1i(glGetUniformLocation(program, "gridEnabled"), 0);
        } else {
            // draw_model_force_rgb(program, *obj, 0.8, 0.6, 0.4);
            draw_model(program, *obj, pass);
        }
    }
}

void initialize_shadow_map_fbo(GLuint *fbo, GLuint *tex, Light light)
{
    // TODO: remember to free resources
    glGenFramebuffers(1, fbo);
    glGenTextures(1, tex);

    GLuint tex_type = light.type == POINTLIGHT ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

    glBindTexture(tex_type, *tex);
        if (light.type == POINTLIGHT) {
            for (int i = 0; i < 6; i++) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                             SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 0,
                             GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            }
            glTexParameteri(tex_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(tex_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(tex_type, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        } else {
            glTexImage2D(tex_type, 0, GL_DEPTH_COMPONENT16, SHADOW_MAP_RESOLUTION,
                         SHADOW_MAP_RESOLUTION, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            glTexParameteri(tex_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(tex_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            // This border color is needed to remove artifacts at the edge of the light's
            // view frustrum.
            vec4 border_color = { 1.0f, 1.0f, 1.0f, 1.0f };
            glTexParameterfv(tex_type, GL_TEXTURE_BORDER_COLOR, border_color);
        }
        
        glTexParameteri(tex_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(tex_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(tex_type, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    // we only need the depth buffer
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void shadow_mapping_pass(int width, int height, GLuint fbo, GLuint tex,
                         Object **scene_geometry, int obj_count,
                         GLuint program, Light *light)
{
    mat4 proj_mat, view_mat;
    glm_perspective(GLM_PI_2f, 1, 0.01f, FAR_PLANE, proj_mat);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    switch (light->type) {
    case SPOTLIGHT:
    case DIRECTIONAL: {
		vec3 up = { 0, 1, 0 };
		vec3 target = { 5, 0, 5 };
		glm_lookat(light->pos, target, up, view_mat);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
        render_scene(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION,
                     0, 0, light->pos, light, proj_mat, view_mat, scene_geometry,
                     obj_count, program, PASS_SHADOW_MAP);
        break;
    }
    case POINTLIGHT: {
        vec3 directions[6] = {
            {1.0f, 0.0f, 0.0f},
            {-1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, -1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, -1.0f},
        };
        vec3 up[6] = {
            {0.0f, -1.0f, 0.0f},
            {0.0f, -1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, -1.0f, 0.0f},
            {0.0f, -1.0f, 0.0f},
        };

        for (int i = 0; i < 6; i++) {
            vec3 camera_target;
            glm_vec3_add(light->pos, directions[i], camera_target);
            glm_lookat(light->pos, camera_target, up[i], view_mat);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, tex, 0);
            render_scene(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION,
                         0, 0, light->pos, light, proj_mat, view_mat,
                         scene_geometry, obj_count, program, PASS_SHADOW_MAP);
        }
        break;
    }
    default:
        assert(false && "UNKNOWN LIGHT TYPE!");
        break;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void final_render(float width, float height, float mouse_x, float mouse_y,
                  Camera camera, Light *light, Object **scene_geometry,
                  int obj_count, GLuint program, GLuint shadow_map_tex,
                  GLuint dither_tex)
{
    GLuint tex_type = light->type == POINTLIGHT ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(tex_type, shadow_map_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, dither_tex);

    render_scene(width, height, mouse_x, mouse_y, camera.pos,
                 light, camera.proj_mat, camera.view_mat,
                 scene_geometry, obj_count, program, PASS_FINAL);

    glBindTexture(GL_TEXTURE_2D, 0);
}

Light create_light(LightType type, float x, float y, float z,
                   float dir_x, float dir_y, float dir_z)
{
    Light light = {
        type,
        {x, y, z},
        {dir_x, dir_y, dir_z}
    };
    return light;
}

Camera create_targeted_camera(vec3 pos, vec3* target)
{
    Camera camera = {};
    camera.type = CAMERA_TARGETED;
    glm_vec3_copy(pos, camera.pos);
    vec3 up = { 0.0, 1.0, 0.0 };
    //vec3 up = { 0.0, 0.0, -1.0 };
    glm_vec3_copy(up, camera.up);
    camera.target = target;
    return camera;
}

void update_camera_matrices(int width, int height, Camera* camera)
{
    glm_lookat(camera->pos, *camera->target, camera->up, camera->view_mat);
    glm_perspective(GLM_PI_4f, (float)width / height, 0.01f, FAR_PLANE, camera->proj_mat);
}

int main(int argc, char** argv)
{
    GLFWwindow* window;
    GLuint vertex_shader, fragment_shader, program;
    GLint mvp_location, model_mat_location;
    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Engine Propria", NULL, NULL);

    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window, key_callback);
    glfwMakeContextCurrent(window);

    glewExperimental = 1;
    if (glewInit() != GLEW_OK) {
        exit(EXIT_FAILURE);
    }

    // display OpenGL context version
    {
        const GLubyte* version_str = glGetString(GL_VERSION);
        printf("%s\n", version_str);
    }

    glfwSwapInterval(0); // TODO: check

    // TODO: fix the size
    GLchar shader_info_buffer[200];
    GLint shader_info_len;

    // default shader program
    {
        GLuint vert = compile_shader(GL_VERTEX_SHADER, "shaders/vert.glsl");
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, "shaders/frag.glsl");
        program = create_program(vert, frag);
    }

    // load models
    int monkey_id = loadModel("assets/monkey.obj", NULL, VERTEX_TEXTURE, false);
    int man_id = loadModel("assets/man.obj", NULL, VERTEX_TEXTURE, false);
    //int man_id = loadModel("assets/xbot.fbx", NULL, VERTEX_TEXTURE, false);
#if 0
#if 0
    int plane_id = loadModel("assets/plane.obj", "assets/ground2.jpg", VERTEX_TEXTURE, true);
    model_add_normal_map(&loaded_models[plane_id], "assets/ground2_normal_map3.jpg");
#else
    int plane_id = loadModel("assets/plane.obj", "assets/brickwall_test.jpg", VERTEX_TEXTURE, true);
    model_add_normal_map(&loaded_models[plane_id], "assets/brickwall_normal.jpg");
#endif
#else
    int num_tiles = 50;
    float tile_size = 1.0f;
    int plane_id = create_tiled_plane(num_tiles, tile_size, 1/50.0f, "assets/brickwall_test.jpg");
    //model_add_normal_map(&loaded_models[plane_id], "assets/brickwall_normal.jpg");
#endif

    // initialize scene geometry
	Object man = create_object(OBJ_CHARACTER, man_id, 0, 0, 0, 5, 3.0, 2);
	Object man2 = create_object(OBJ_CHARACTER, man_id, 5, 0, 3, 5, 3.0, 2);
	Object plane = create_object(OBJ_GROUND, plane_id, 0, 0, 0, 0, 1, 256);
    //plane.scale_tex_coords = 88.0;
    Object *scene_geometry[] = { &man, &man2, &plane };
    int obj_count = sizeof(scene_geometry) / sizeof(*scene_geometry);

    // initialize camera data
    Camera camera;
    {
        vec3 camera_pos = { 0, 35, 12 };
        camera = create_targeted_camera(camera_pos, &man.pos);
    }

    // initialize light data
    Light light = create_light(POINTLIGHT,   0, 8.0, 8.0,   0, 0, 0);

    float delta_time = glfwGetTime();
    float last_time = glfwGetTime();

    POLL_GL_ERROR;

    GLuint shadow_map_fbo, shadow_map_tex;
    initialize_shadow_map_fbo(&shadow_map_fbo, &shadow_map_tex, light);
    GLuint shadow_map_program = create_shadow_map_program();

    const GLchar dither_pattern[] = {
        0, 32,  8, 40,  2, 34, 10, 42,
        48, 16, 56, 24, 50, 18, 58, 26,
        12, 44,  4, 36, 14, 46,  6, 38,
        60, 28, 52, 20, 62, 30, 54, 22,
        3, 35, 11, 43,  1, 33,  9, 41,
        51, 19, 59, 27, 49, 17, 57, 25,
        15, 47,  7, 39, 13, 45,  5, 37,
        63, 31, 55, 23, 61, 29, 53, 21
    };

    GLuint dither_tex;
    glGenTextures(1, &dither_tex);
    glBindTexture(GL_TEXTURE_2D, dither_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 8, 8, 0, GL_RED, GL_UNSIGNED_BYTE, dither_pattern);
    glBindTexture(GL_TEXTURE_2D, 0);

    float last_fps_update = glfwGetTime();
    int num_frames = 0;

    POLL_GL_ERROR;
    while (!glfwWindowShouldClose(window)) {
        delta_time = glfwGetTime() - last_time;
        last_time += delta_time;

        // Measure FPS
        double currentTime = glfwGetTime();
        num_frames++;
        if (currentTime - last_fps_update >= 1.0) {
            printf("%f ms/frame (%f FPS)\n", 1000.0 / double(num_frames), double(num_frames));
            num_frames = 0;
            last_fps_update += 1.0;
        }

        // input handling
        vec3 dir = { 0 };
        if (glfwGetKey(window, GLFW_KEY_W)) {
            dir[2] = -1;
        }
        if (glfwGetKey(window, GLFW_KEY_A)) {
            dir[0] = -1;
        }
        if (glfwGetKey(window, GLFW_KEY_S)) {
            dir[2] = 1;
        }
        if (glfwGetKey(window, GLFW_KEY_D)) {
            dir[0] = 1;
        }
        glm_vec3_normalize(dir);
        glm_vec3_scale(dir, man.speed * delta_time, dir);
        glm_vec3_add(man.pos, dir, man.pos);
        glm_vec3_add(camera.pos, dir, camera.pos);

        // handle screen resize
        // TODO: don't do this every frame, only when it changes!!
        float ratio;
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        ratio = width / (float)height;

        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        xpos /= width / 2;
        ypos /= height / 2;
        xpos -= 1;
        ypos -= 1;
        double nds_x = xpos;
        double nds_y = -ypos;
        xpos *= ratio;
        //printf("%lf %lf\n", xpos, ypos);

        update_camera_matrices(width, height, &camera);

        // mouse picking for light position
        vec3 ray_origin, ray_dir;
        screen_to_world_space_ray(camera.pos, nds_x, nds_y,
                                  camera.proj_mat, camera.view_mat,
                                  ray_origin, ray_dir);
        vec3 plane_normal = { 0.0, 1.0, 0.0 };
        vec3 target_pos;
        ray_plane_intersection(ray_origin, ray_dir, plane_normal, 0.0f, target_pos);
        float light_y = light.pos[1];
        glm_vec3_copy(target_pos, light.pos);
        light.pos[1] = light_y;

        // mouse picking for raising/lowering terrain
        // TODO: continue from here, debug normals
        if (glfwGetMouseButton(window, 0)) {
            vec3 ray_origin, ray_dir;
            screen_to_world_space_ray(camera.pos, nds_x, nds_y,
                                      camera.proj_mat, camera.view_mat,
                                      ray_origin, ray_dir);
            vec3 plane_normal = { 0.0, 1.0, 0.0 };
            vec3 target_pos;
            ray_plane_intersection(ray_origin, ray_dir, plane_normal, 0.0f, target_pos);

            Model *plane_model = &loaded_models[plane_id];
            int width = num_tiles * 6;
            float area = 3.1f;
            float steepness = 0.6f;
            for (float step = -area; step < area; step += tile_size) {
                for (float s2 = -area; s2 < area; s2 += tile_size) {
                    int x = (target_pos[0] + s2) / tile_size;
                    int z = (target_pos[2] + step) / tile_size;

                    float dist = sqrt(s2 * s2 + step * step) * steepness;
                    float level_offset = 0.1f * area / (dist * dist + area);

                    // TODO: adjust normals and tex coords
                    plane_model->vertices[z * width + x * 6 + 2][1] += level_offset;
                    if (x > 0) plane_model->vertices[z * width + (x - 1) * 6 + 1][1] += level_offset;
                    if (x > 0) plane_model->vertices[z * width + (x - 1) * 6 + 5][1] += level_offset;
                    if (z > 0) plane_model->vertices[(z - 1) * width + x * 6 + 0][1] += level_offset;
                    if (z > 0) plane_model->vertices[(z - 1) * width + x * 6 + 3][1] += level_offset;
                    if (x > 0 && z > 0) plane_model->vertices[(z - 1) * width + (x - 1) * 6 + 4][1] += level_offset;
                }
            }

            // TODO: fix the corner artifact, and the shadow acne!
            // TODO: use slightly larger area to smoothen edges of raised/lowered terrain
            // TODO: fix corner cases (check boundaries)
            // TODO: fix corner cases (fix the artifact at the edges of the map)
            // has to be done only after all vertices were translated
            area += tile_size * 15;
            for (float step = -area; step < area; step += tile_size) {
                for (float s2 = -area; s2 < area; s2 += tile_size) {
                    int x = (target_pos[0] + s2) / tile_size;
                    int z = (target_pos[2] + step) / tile_size;

#define P(X,Z) ((Z) * width + (X) * 6 + 2)
#define V(X,Z) plane_model->vertices[P((X),(Z))]
                    // TODO: conditionals on border
                    vec3 O, A, B, C, D, E, F;
                    glm_vec3_copy(V(x, z), O);
                    // fprintf(stderr, "x %d, z %d, idx %d\n", x, z, P(x, z));

                    glm_vec3_copy(V(x, z-1), A);
                    glm_vec3_copy(V(x+1, z-1), B);
                    glm_vec3_copy(V(x+1, z), C);
                    glm_vec3_copy(V(x, z+1), D);
                    glm_vec3_copy(V(x-1, z+1), E);
                    glm_vec3_copy(V(x-1, z), F);
                    
                    // TODO: should we care aboutthe magnitude of these vectors? or should we normalize them?
                    vec3 OA, OB, OC, OD, OE, OF;
                    glm_vec3_sub(A, O, OA);
                    glm_vec3_sub(B, O, OB);
                    glm_vec3_sub(C, O, OC);
                    glm_vec3_sub(D, O, OD);
                    glm_vec3_sub(E, O, OE);
                    glm_vec3_sub(F, O, OF);

#if 0
                    glm_vec3_normalize(OA);
                    glm_vec3_normalize(OB);
                    glm_vec3_normalize(OC);
                    glm_vec3_normalize(OD);
                    glm_vec3_normalize(OE);
                    glm_vec3_normalize(OF);
#endif

                    vec3 N1, N2, N3, N4, N5, N6;
                    glm_vec3_cross(OB, OA, N1);
                    glm_vec3_cross(OC, OA, N2);
                    glm_vec3_cross(OD, OC, N3);
                    glm_vec3_cross(OE, OD, N4);
                    glm_vec3_cross(OF, OE, N5);
                    glm_vec3_cross(OA, OF, N6);

#if 0
                    glm_vec3_normalize(N1);
                    glm_vec3_normalize(N2);
                    glm_vec3_normalize(N3);
                    glm_vec3_normalize(N4);
                    glm_vec3_normalize(N5);
                    glm_vec3_normalize(N6);
#endif

                    vec3 R = GLM_VEC3_ZERO_INIT;
                    glm_vec3_add(R, N1, R);
                    glm_vec3_add(R, N2, R);
                    glm_vec3_add(R, N3, R);
                    glm_vec3_add(R, N4, R);
                    glm_vec3_add(R, N5, R);
                    glm_vec3_add(R, N6, R);

                    glm_vec3_normalize(R);
#if 1
                    //smooth
                    glm_vec3_copy(R, plane_model->normals[z * width + x * 6 + 2]);
                    glm_vec3_copy(R, plane_model->normals[z * width + (x-1) * 6 + 1]);
                    glm_vec3_copy(R, plane_model->normals[z * width + (x-1) * 6 + 5]);
                    glm_vec3_copy(R, plane_model->normals[(z-1) * width + x * 6 + 0]);
                    glm_vec3_copy(R, plane_model->normals[(z-1) * width + x * 6 + 3]);
                    glm_vec3_copy(R, plane_model->normals[(z-1) * width + (x-1) * 6 + 4]);
#else
                    //flat
                    glm_vec3_copy(N3, plane_model->normals[z * width + x * 6 + 2]);
                    glm_vec3_copy(N5, plane_model->normals[z * width + (x - 1) * 6 + 1]);
                    glm_vec3_copy(N4, plane_model->normals[z * width + (x-1) * 6 + 5]);
                    glm_vec3_copy(N1, plane_model->normals[(z-1) * width + x * 6 + 0]);
                    glm_vec3_copy(N2, plane_model->normals[(z-1) * width + x * 6 + 3]);
                    glm_vec3_copy(N6, plane_model->normals[(z-1) * width + (x-1) * 6 + 4]);
#endif
#undef P
#undef V
                }
            }

            // upload new data
            // TODO: cleanup
            static GLuint vbo = -1, vbo2 = -1;
            if (vbo == -1) {
                // TODO: free
                glGenBuffers(1, &vbo);
                glGenBuffers(1, &vbo2);
            }
            // TODO: use buffersubdata
            glBindVertexArray(plane.vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, plane_model->num_faces * 3 * sizeof(vec3), plane_model->vertices, GL_STREAM_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
            glBindBuffer(GL_ARRAY_BUFFER, vbo2);
            glBufferData(GL_ARRAY_BUFFER, plane_model->num_faces * 3 * sizeof(vec3), plane_model->normals, GL_STREAM_DRAW);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
            glBindVertexArray(0);
        }

        // update player direction
        man.dir[0] = xpos;
        man.dir[1] = ypos;
        glm_vec2_normalize(man.dir);

        // shadow mapping
        shadow_mapping_pass(width, height, shadow_map_fbo, shadow_map_tex,
                            scene_geometry, obj_count, shadow_map_program, &light);

#if 1
        // render actual scene
        final_render(width, height, nds_x, nds_y, camera,
                     &light, scene_geometry, obj_count, program,
                     shadow_map_tex, dither_tex);
#else
        POLL_GL_ERROR;
        // blit shadow map to screen quad
        //blit_texture(width, height, shadow_map_tex);
        blit_texture(width, height, loaded_models[plane_id].normal_map_id);
#endif

        // present
        glfwSwapBuffers(window);
        POLL_GL_ERROR;
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwPollEvents();

    return 0;
}
