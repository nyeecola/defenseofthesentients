#version 330

// TODO: separate view_proj ???
uniform mat4 model;
uniform mat4 view_proj;
uniform mat4 shadow_map_matrix; // for shadow mapping

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;

smooth out vec3 normal;
smooth out vec3 fragPos;
//smooth out vec4 fragPosFromLight;

void main() {
    gl_Position = view_proj * model * vec4(vPos, 1.0);
    fragPos = vec3(model * vec4(vPos, 1.0));
    normal = mat3(transpose(inverse(model))) * vNormal; //TODO: do this on CPU
    //fragPosFromLight = shadow_map_matrix * vec4(fragPos, 1.0);
}
