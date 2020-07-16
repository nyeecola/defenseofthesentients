#version 330

uniform vec3 lightPos;
uniform vec3 cameraPos;

in vec3 normal;
in vec3 fragPos;

void main() {
	vec3 objColor = vec3(1.0, 0.7, 0.5);
	vec3 lightColor = vec3(1.0);

    vec3 ambientContrib = 0.1 * lightColor;

	vec3 norm = normalize(normal);
	vec3 lightDir = normalize(lightPos - fragPos);  

	float diffuseTmp = max(dot(norm, lightDir), 0.0);
	vec3 diffuseContrib = diffuseTmp * lightColor;

	// TODO: test specular highlight
	float specularTmp = 0.5;
	vec3 cameraDir = normalize(cameraPos - fragPos);
	vec3 reflectDir = reflect(-lightDir, norm); 

	specularTmp = specularTmp * pow(max(dot(cameraDir, reflectDir), 0.0), 32);
	vec3 specularContrib = specularTmp * lightColor;

	vec3 result = (ambientContrib + diffuseContrib + specularContrib) * objColor;
	// TODO: should we cap at 1?
	gl_FragColor = vec4(result, 1.0);
};
