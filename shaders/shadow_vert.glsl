#version 330

// TODO: separate view_proj ???
uniform mat4 model;
uniform mat4 view_proj;

layout (location = 0) in vec3 vPos;

void main() {
    gl_Position = view_proj * model * vec4(vPos, 1.0);
}
