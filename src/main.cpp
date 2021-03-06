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

#include "defines.h"
#include "engine.h"

#include "globals.cpp"
#include "model.cpp"

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

int main(int argc, char** argv) {
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

    // vertex shader
    {
        vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, (const char* const*)&vertex_shader_text, NULL);
        glCompileShader(vertex_shader);

        glGetShaderInfoLog(vertex_shader, 200, &shader_info_len, shader_info_buffer);

        if (shader_info_len) printf("Vertex Shader: %s\n", shader_info_buffer);
    }

    // frag shader
    {
        fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, (const char* const*)&fragment_shader_text, NULL);
        glCompileShader(fragment_shader);
        glGetShaderInfoLog(fragment_shader, 200, &shader_info_len, shader_info_buffer);
        if (shader_info_len) printf("Fragment Shader: %s\n", shader_info_buffer);
    }

    // shader program
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
    int plane_id = loadModel("assets/plane.obj", NULL, VERTEX_TEXTURE, 1024);

    // create player object
    Object man = create_object(man_id, 0, 0, 0, 5, 2, 2);
    Object man2 = create_object(man_id, 5, 0, 3, 5, 2, 2);

    // create placeholder ground object
    Object plane = create_object(plane_id, 0, 0, 0, 0, 1, 256);

    // initialize camera data
    vec3 camera_pos = { 0, 35, 12 };
    vec3 camera_up = { 0, 0, -1 }; /* TODO: positive or negative? */

    float delta_time = glfwGetTime();
    float last_time = glfwGetTime();

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

        /* handle screen resize */
        // TODO: don't do this every frame, only when it changes!!
        float ratio;
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        ratio = width / (float)height;
        mat4 proj_mat;
        glm_perspective(GLM_PI_4f, ratio, 0.01f, 300.0f, proj_mat);


        /* input handling */
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

        /* update player direction */
        man.dir[0] = xpos;
        man.dir[1] = ypos;
        glm_vec2_normalize(man.dir);


        mat4 view_mat;
        vec3 camera_target = { man.pos[0], 0, man.pos[2] + 0.2 };
        glm_lookat(camera_pos, camera_target, camera_up, view_mat);

        mat4 view_proj;
        glm_mat4_identity(view_proj);
        glm_mat4_mul(proj_mat, view_mat, view_proj);


        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glViewport(0, 0, width, height);
        glClearColor(0.0, 0.0, 0.0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        vec3 light_pos = { -5, 5, 10 };

        glUseProgram(program);

        glUniform3fv(glGetUniformLocation(program, "lightPos"), 1, light_pos);
        glUniform3fv(glGetUniformLocation(program, "cameraPos"), 1, camera_pos);
        glUniformMatrix4fv(glGetUniformLocation(program, "view_proj"), 1, GL_FALSE, (const GLfloat*)view_proj);

        // draw man
        draw_model(program, man);

        // draw NPC
        draw_model(program, man2);

        // draw plane
        glUniform1i(glGetUniformLocation(program, "gridEnabled"), grid_enabled);
        // TODO: deal with this
        vec4 ray_clip = { nds_x, nds_y, -1, 1 };
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
        draw_model_force_rgb(program, plane, 0.8, 0.7, 0.7);
        glUniform1i(glGetUniformLocation(program, "gridEnabled"), 0);

        glfwSwapBuffers(window);
        POLL_GL_ERROR;
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwPollEvents();

    return 0;
}
