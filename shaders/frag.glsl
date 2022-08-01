#version 330

#define GRID_CELL_SIZE 0.8
#define GRID_LINE_WEIGHT 0.08
#define GRID_HIGHLIGHT_SIZE 12

#define GRID_CELL_BORDER_COLOR (vec3(40.0, 117.0, 188.0)/255.0)

uniform vec3 lightPos;
uniform vec3 cameraPos;
uniform bool hasTexture;

uniform bool forceColor;
uniform vec3 forcedColor;

uniform bool gridEnabled;
uniform vec3 cursorPos;

uniform int shininess;

uniform sampler2D shadowMap;
uniform sampler2D ditherPattern;

smooth in vec3 normal;
smooth in vec3 fragPos;
smooth in vec4 fragPosFromLight;

float is_shadowed(vec4 pos_from_light) {
    // perspective divide (not done automatically because it's not gl_Position)
    vec3 pos = pos_from_light.xyz / pos_from_light.w;

    // [-1,1] -> [0,1]
    pos = pos / 2 + 0.5;

    // point outside the light's frustrum
    if (pos.z > 1.0)
        return 0.0;

    // get first occluder from light's POV
    float occluder = texture(shadowMap, pos.xy).r;

    //return 0.7;
    return pos.z > occluder ? 1.0 : 0.0;
}

vec3 dither(vec3 og_color)  {
    vec3 offset = vec3(texture(ditherPattern, gl_FragCoord.xy / 8.0).r / 32.0 - (1.0 / 128.0));
    return og_color + offset;
}

void main() {
    //vec3 objColor = vec3(1.0, 0.8, 0.6); // default color when no texture is passed
    vec3 objColor = vec3(0.7, 0.4, 0.08);
    if (hasTexture) {
        // objColor = tex...
    }

    /*
    if (forceColor) {
        objColor = forcedColor;
    }
    */

    //vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 lightColor = vec3(1.0, 1.0, 1.0) * 5.0;

    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightPos - fragPos);
    float distance = length(lightPos - fragPos);

    // ambient
    vec3 ambientContrib = 0.005 * lightColor;

    // diffuse
    float diffuseTmp = max(dot(norm, lightDir), 0.0);
    vec3 diffuseContrib = diffuseTmp * lightColor;

    // specular (TODO: review this)
    /*
    float specularTmp = 0.5;
    vec3 cameraDir = normalize(cameraPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm); 
    specularTmp = specularTmp * pow(max(dot(cameraDir, reflectDir), 0.0), shininess);
    vec3 specularContrib = specularTmp * lightColor;
    */

    float attenuation = 1.0 / (distance * distance);
    
    vec3 result = ((diffuseContrib * (1.0 - is_shadowed(fragPosFromLight)) * attenuation) + ambientContrib) * objColor;

    /*
    // draw grid
    if (gridEnabled) {
        float dist = length(cursorPos - fragPos);
        vec3 targetColor = mix(GRID_CELL_BORDER_COLOR, result, smoothstep(0.0, GRID_HIGHLIGHT_SIZE, float(dist)));
        float x_offset = abs(fragPos.x - round(fragPos.x / GRID_CELL_SIZE) * GRID_CELL_SIZE);
        float z_offset = abs(fragPos.z - round(fragPos.z / GRID_CELL_SIZE) * GRID_CELL_SIZE);
        result = mix(targetColor, result, smoothstep(0.0, GRID_LINE_WEIGHT, abs(fragPos.x - round(fragPos.x / GRID_CELL_SIZE) * GRID_CELL_SIZE)));
        result = mix(targetColor, result, smoothstep(0.0, GRID_LINE_WEIGHT, abs(fragPos.z - round(fragPos.z / GRID_CELL_SIZE) * GRID_CELL_SIZE)));
    }
    */

    // gamma correction
    result = pow(result, vec3(1.0/2.2));

    // TODO: should we cap at 1?
    gl_FragColor = vec4(dither(result), 1.0);
    //gl_FragColor = vec4(result, 1.0);
}
