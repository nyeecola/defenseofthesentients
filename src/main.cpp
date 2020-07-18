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

    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        if (selected != -1) {
            selected = -1;
            camera_enabled = true;
        }
        else {
            camera_enabled = !camera_enabled;
        }
    }

    for (int i = 0; i <= 9; i++) {
        int state = glfwGetKey(window, GLFW_KEY_0 + i);
        if (state == GLFW_PRESS) {
            selected = i;
            camera_enabled = false;
        }
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

    glfwSwapInterval(1); // TODO: check

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

    // TODO: when can I reuse this?
    GLuint vbo1, vbo2;
    glGenBuffers(1, &vbo1);
    glGenBuffers(1, &vbo2);

    // init monkey
    int monkey_id = loadModel("assets/monkey.obj", NULL, VERTEX_TEXTURE, 1024);

    // init object array
    glGenVertexArrays(1, &monkey.vao);
    monkey.model_id = monkey_id;
    glm_mat4_identity(monkey.mat);
    glm_scale_uni(monkey.mat, 0.6); /* this is slightly dangerous, be careful to not change scale after startup */

    glBindVertexArray(monkey.vao);
        // NOTE: OpenGL error checks have been omitted for brevity
        glBindBuffer(GL_ARRAY_BUFFER, vbo1);
        glBufferData(GL_ARRAY_BUFFER, loaded_models[monkey.model_id].num_faces * 3 * sizeof(vec3), loaded_models[monkey.model_id].vertices, GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

        glBindBuffer(GL_ARRAY_BUFFER, vbo2);
        glBufferData(GL_ARRAY_BUFFER, loaded_models[monkey.model_id].num_faces * 3 * sizeof(vec3), loaded_models[monkey.model_id].normals, GL_STREAM_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
    glBindVertexArray(0);

    POLL_GL_ERROR;

    vec3 camera_pos = { 0, 0, -5 };

    float delta_time = glfwGetTime();
    float last_time = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        POLL_GL_ERROR;

        delta_time = glfwGetTime() - last_time;
        last_time += delta_time;

        float ratio;
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        ratio = width / (float)height;
        mat4 proj_mat;
        glm_perspective(GLM_PI_4f, ratio, 0.01f, 300.0f, proj_mat);


        mat4 view_mat;
        glm_mat4_identity(view_mat);
        glm_translate(view_mat, camera_pos);

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

        vec3 monkey_pos = { 0, 0, 2 };
        obj_translate(monkey, monkey_pos);

        draw_model(program, monkey);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwPollEvents();

    return 0;
}