#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

// PBR textures passed from external code
uniform sampler2D albedoMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;

uniform vec3 lightDir;
uniform vec3 viewPos;

void main() {
    // Sample data from textures
    vec3 albedo = texture(albedoMap, TexCoords).rgb;
    float metallic = texture(metallicMap, TexCoords).r;
    float roughness = texture(roughnessMap, TexCoords).r;
    float ao = texture(aoMap, TexCoords).r;

    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V + L);

    // Ambient lighting
    vec3 ambient = vec3(0.3) * albedo * ao; 

    // Diffuse lighting
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * albedo * (1.0 - metallic); 

    // Specular reflection
    float spec = pow(max(dot(N, H), 0.0), mix(2.0, 256.0, 1.0 - roughness));
    vec3 specular = vec3(1.0) * spec * mix(0.1, 1.0, metallic);

    // Final composition
    vec3 result = ambient + diffuse + specular;

    // Gamma correction adjustment
    result = pow(result, vec3(1.0/1.2));

    FragColor = vec4(result, 1.0);
}