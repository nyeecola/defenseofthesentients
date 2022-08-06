#version 330

// TODO: separate view_proj ???
uniform mat4 model;
uniform mat4 view_proj;

layout (location = 0) in vec3 vPos;

out vec4 fragPos;

void main() {
    fragPos = model * vec4(vPos, 1.0);
    gl_Position = view_proj * fragPos;
}
