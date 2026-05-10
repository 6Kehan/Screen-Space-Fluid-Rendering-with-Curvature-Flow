#version 330 core
out vec4 FragColor;
in vec3 WorldPos;

uniform sampler2D equirectangularMap;
uniform samplerCube environmentMap;
uniform int mode; // 0: convert, 1: render
const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {		
    if(mode == 0) {
        vec2 uv = SampleSphericalMap(normalize(WorldPos));
        vec3 color = texture(equirectangularMap, uv).rgb;
        FragColor = vec4(color, 1.0);
    } else {
        vec3 envColor = texture(environmentMap, WorldPos).rgb;
        envColor = envColor / (envColor + vec3(1.0));
        envColor = pow(envColor, vec3(1.0/2.2));
        FragColor = vec4(envColor, 1.0);
    }
}