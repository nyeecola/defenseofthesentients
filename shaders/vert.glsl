#version 330

// TODO: separate view_proj ???
uniform mat4 model;
uniform mat4 view_proj;
uniform mat4 shadow_map_matrix; // for shadow mapping

uniform float scaleTexCoords;

uniform bool hasNormalMap;

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoords;
layout (location = 3) in vec3 vTangent;
layout (location = 4) in vec3 vBitangent;

smooth out vec3 normal;
smooth out vec3 fragPos;
smooth out vec2 texCoords;
//smooth out vec4 fragPosFromLight;

out mat3 TBN;

void main() {
    gl_Position = view_proj * model * vec4(vPos, 1.0);
    fragPos = vec3(model * vec4(vPos, 1.0));
    if (hasNormalMap) {
        vec3 T = normalize(vec3(model * vec4(vTangent, 0.0)));
        vec3 B = normalize(vec3(model * vec4(vBitangent, 0.0)));
        vec3 N = normalize(vec3(model * vec4(vNormal, 0.0)));
        TBN = mat3(T, B, N);
	} else {
        normal = mat3(transpose(inverse(model))) * vNormal; //TODO: do this on CPU
	}
    texCoords = vTexCoords * scaleTexCoords;
    //fragPosFromLight = shadow_map_matrix * vec4(fragPos, 1.0);
}
