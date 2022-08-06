#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#define CGLM_ALL_UNALIGNED
#include <cglm/cglm.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "defines.h"
#include "engine.h"

#include "globals.cpp"
#include "model.cpp"
//#include "model2.cpp"

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

// HACK
mat4 shadow_map_matrix;

// TODO: put all this state in a struct
void render_scene(float width, float height, float mouse_x, float mouse_y,
                  vec3 camera_pos, vec3 camera_up, vec3 light_pos, mat4 view_mat,
                  Object man, Object man2, Object plane, GLuint program, bool final) {
    float ratio = width / height;

    // TODO: move this out
    float far_plane = 300.0f;

    mat4 proj_mat;
    if (final)
        glm_perspective(GLM_PI_4f, ratio, 0.01f, far_plane, proj_mat);
    else
        glm_perspective(GLM_PI_2f, ratio, 0.01f, far_plane, proj_mat);

    mat4 view_proj;
    glm_mat4_identity(view_proj);
    glm_mat4_mul(proj_mat, view_mat, view_proj);

    glEnable(GL_DEPTH_TEST);
    if (final) {
        glDisable(GL_CULL_FACE); // TODO: reenable
        glCullFace(GL_BACK);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
    }
    glViewport(0, 0, width, height);
    glClearColor(0.0, 0.0, 0.0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(program);

    if (final) {
        glUniform1i(glGetUniformLocation(program, "shadowMap"), 0);
        glUniformMatrix4fv(glGetUniformLocation(program, "shadow_map_matrix"), 1, GL_FALSE, (const GLfloat*)shadow_map_matrix);
        glUniform1i(glGetUniformLocation(program, "ditherPattern"), 1);
    } else {
        glm_mat4_copy(view_proj, shadow_map_matrix);
    }
    glUniform3fv(glGetUniformLocation(program, "lightPos"), 1, light_pos);
    glUniform1f(glGetUniformLocation(program, "farPlane"), far_plane);
    glUniform3fv(glGetUniformLocation(program, "cameraPos"), 1, camera_pos);
    glUniformMatrix4fv(glGetUniformLocation(program, "view_proj"), 1, GL_FALSE, (const GLfloat*)view_proj);

    // draw man
    draw_model(program, man);

    // draw NPC
    draw_model(program, man2);

    // draw plane
    if (final) { // we don't care about the grid when doing shadow mapping
        // TODO: refactor
        glUniform1i(glGetUniformLocation(program, "gridEnabled"), grid_enabled);
        vec4 ray_clip = {mouse_x, mouse_y, -1, 1};
        mat4 proj_mat_inv;
        glm_mat4_inv(proj_mat, proj_mat_inv);
        glm_mat4_mulv(proj_mat_inv, ray_clip, ray_clip);
        vec4 ray_eye = { ray_clip[0], ray_clip[1], -1, 0 };
        mat4 view_mat_inv;
        glm_mat4_inv(view_mat, view_mat_inv);
        glm_mat4_mulv(view_mat_inv, ray_eye, ray_eye);
        vec3 ray_world = { ray_eye[0], ray_eye[1], ray_eye[2] };
        glm_vec3_normalize(ray_world);
        float t = -camera_pos[1] / ray_world[1];
        vec3 target_pos = { camera_pos[0] + t * ray_world[0], 0, camera_pos[2] + t * ray_world[2] };
        glUniform3fv(glGetUniformLocation(program, "cursorPos"), 1, target_pos);
    }
    draw_model_force_rgb(program, plane, 0.8, 0.7, 0.7);
    glUniform1i(glGetUniformLocation(program, "gridEnabled"), 0);
}

void initialize_shadow_map_fbo(GLuint *fbo, GLuint *tex, LightType light_type) {
    // TODO: remember to free resources
    glGenFramebuffers(1, fbo);
    glGenTextures(1, tex);

    GLuint tex_type = light_type == POINTLIGHT ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

    glBindTexture(tex_type, *tex);
        if (light_type == POINTLIGHT) {
            for (int i = 0; i < 6; i++) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                             SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 0,
                             GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            }
        } else {
            glTexImage2D(tex_type, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_RESOLUTION,
                         SHADOW_MAP_RESOLUTION, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }
        
        glTexParameteri(tex_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(tex_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(tex_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(tex_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        if (light_type == POINTLIGHT)
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);

        // This border color is needed to remove artifacts at the edge of the light's
        // view frustrum.
        vec4 border_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(tex_type, GL_TEXTURE_BORDER_COLOR, border_color);
    glBindTexture(tex_type, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    // we only need the depth buffer
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void shadow_mapping_pass(GLuint fbo, GLuint tex, Object man, Object man2, Object plane,
                         GLuint program, vec3 light_pos, vec3 camera_up, LightType light_type)
{
    
    mat4 view_mat;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    switch (light_type) {
    case SPOTLIGHT:
    case DIRECTIONAL: {
        vec3 camera_target = { 5, 0, 5 };
        glm_lookat(light_pos, camera_target, camera_up, view_mat);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
        render_scene(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION,
                     0, 0, light_pos, camera_up, light_pos, view_mat,
                     man, man2, plane, program, false);
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
            glm_vec3_add(light_pos, directions[i], camera_target);
            glm_lookat(light_pos, camera_target, up[i], view_mat);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, tex, 0);
            render_scene(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION,
                         0, 0, light_pos, camera_up, light_pos, view_mat,
                         man, man2, plane, program, false);
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
                  vec3 camera_pos, vec3 camera_up, vec3 light_pos,
                  LightType light_type, Object man, Object man2, Object plane,
                  GLuint program, GLuint shadow_map_tex, GLuint dither_tex)
{
    GLuint tex_type = light_type == POINTLIGHT ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(tex_type, shadow_map_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, dither_tex);

    mat4 view_mat;
    vec3 camera_target = { man.pos[0], 0, man.pos[2] + 0.2 };
    glm_lookat(camera_pos, camera_target, camera_up, view_mat);

    render_scene(width, height, mouse_x, mouse_y, camera_pos,
                 camera_up, light_pos, view_mat,
                 man, man2, plane, program, true);

    glBindTexture(GL_TEXTURE_2D, 0);
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

    // TODO: free
    char* vertex_shader_text = load_file("shaders/vert.glsl");
    char* fragment_shader_text = load_file("shaders/frag.glsl");

    // default vertex shader
    {
        vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, (const char* const*)&vertex_shader_text, NULL);
        glCompileShader(vertex_shader);

        glGetShaderInfoLog(vertex_shader, 200, &shader_info_len, shader_info_buffer);
        if (shader_info_len) printf("Vertex Shader: %s\n", shader_info_buffer);
    }

    // default frag shader
    {
        fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, (const char* const*)&fragment_shader_text, NULL);
        glCompileShader(fragment_shader);

        glGetShaderInfoLog(fragment_shader, 200, &shader_info_len, shader_info_buffer);
        if (shader_info_len) printf("Fragment Shader: %s\n", shader_info_buffer);
    }

    // default shader program
    {
        program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);

        glGetProgramInfoLog(program, 200, &shader_info_len, shader_info_buffer);
        if (shader_info_len) printf("Shader Program: %s\n", shader_info_buffer);
    }

    // load models
    int monkey_id = loadModel("assets/monkey.obj", NULL, VERTEX_TEXTURE, 1024);
    int man_id = loadModel("assets/man.obj", NULL, VERTEX_TEXTURE, 1024);
    //int man_id = loadModel("assets/xbot.fbx", NULL, VERTEX_TEXTURE, 1024);
    int plane_id = loadModel("assets/plane.obj", NULL, VERTEX_TEXTURE, 1024);

    // create player object
    Object man = create_object(man_id, 0, 0, 0, 5, 2, 2);
    Object man2 = create_object(man_id, 5, 0, 3, 5, 2, 2);

    // create placeholder ground object
    Object plane = create_object(plane_id, 0, 0, 0, 0, 1, 256);

    // initialize camera data
    vec3 camera_pos = { 0, 35, 12 };
    vec3 camera_up = { 0, 0, -1 }; // TODO: positive or negative?

    // initialize light data
    vec3 light_pos = { 0, 5.0, 0 };

    float delta_time = glfwGetTime();
    float last_time = glfwGetTime();

    POLL_GL_ERROR;

    GLuint shadow_map_fbo, shadow_map_tex;
    initialize_shadow_map_fbo(&shadow_map_fbo, &shadow_map_tex, POINTLIGHT);
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
        glm_vec3_add(camera_pos, dir, camera_pos);


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

        // update player direction
        man.dir[0] = xpos;
        man.dir[1] = ypos;
        glm_vec2_normalize(man.dir);
        POLL_GL_ERROR;
        // shadow mapping
        shadow_mapping_pass(shadow_map_fbo, shadow_map_tex, man, man2, plane,
                            shadow_map_program, light_pos, camera_up, POINTLIGHT);

#if 1
        // render actual scene
        final_render(width, height, nds_x, nds_y, camera_pos,
                     camera_up, light_pos, POINTLIGHT, man, man2,
                     plane, program, shadow_map_tex, dither_tex);
#else
        POLL_GL_ERROR;
        // blit shadow map to screen quad
        blit_texture(width, height, shadow_map_tex);
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
