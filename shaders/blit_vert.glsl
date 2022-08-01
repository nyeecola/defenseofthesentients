#version 330

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec2 vTexCoords;
layout (location = 2) in vec3 vColor;

smooth out vec2 texCoords;
smooth out vec3 color;

void main() {
    texCoords = vTexCoords;
    color = vColor;
    gl_Position = vec4(vPos * 2.0 - 1.0, 1.0);
}
