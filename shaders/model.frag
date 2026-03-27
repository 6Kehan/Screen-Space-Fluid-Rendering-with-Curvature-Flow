#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

// 接收外部传来的 PBR 贴图
uniform sampler2D albedoMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;

uniform vec3 lightDir;
uniform vec3 viewPos;

void main() {
    // 从贴图中采样数据
    vec3 albedo = texture(albedoMap, TexCoords).rgb;
    float metallic = texture(metallicMap, TexCoords).r;
    float roughness = texture(roughnessMap, TexCoords).r;
    float ao = texture(aoMap, TexCoords).r;

    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V + L);

    // 1. 环境光 (结合 AO 贴图，让管子的缝隙和阴影处更真实)
    vec3 ambient = vec3(0.3) * albedo * ao; 

    // 2. 漫反射 (金属越强，漫反射越弱，完全金属没有漫反射)
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * albedo * (1.0 - metallic); 

    // 3. 高光反射 (基于粗糙度贴图，粗糙的地方反光散漫，光滑的地方反光尖锐)
    float spec = pow(max(dot(N, H), 0.0), mix(2.0, 256.0, 1.0 - roughness));
    vec3 specular = vec3(1.0) * spec * mix(0.1, 1.0, metallic);

    // 最终合成
    vec3 result = ambient + diffuse + specular;

    // 伽马校正微调（防止过暗）
    result = pow(result, vec3(1.0/1.2));

    FragColor = vec4(result, 1.0);
}