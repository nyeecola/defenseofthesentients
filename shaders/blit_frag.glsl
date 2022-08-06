#version 330

uniform sampler2D tex;

smooth in vec2 texCoords;
smooth in vec3 color;

float linearize_depth(float depth) {
	float ndc = depth * 2.0 - 1.0;
	float near_plane = 0.01f;
	float far_plane = 300.0f;
	return (2.0 * near_plane * far_plane) / (far_plane + near_plane - ndc * (far_plane - near_plane)); // TODO: understand this linearization
}

void main() {
	//float depth = linearize_depth(texture(tex, texCoords).r) / 300.0; // TODO: why?
	float depth = texture(tex, texCoords).r * 4;
	gl_FragColor = vec4(vec3(depth), 1.0);
}
