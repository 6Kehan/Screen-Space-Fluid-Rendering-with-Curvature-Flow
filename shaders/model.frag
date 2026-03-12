#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoords;

uniform vec3 viewPos;
uniform sampler2D equirectangularMap;
uniform sampler2D texture_diffuse1;

uniform bool useTexture;
uniform vec3 baseColor;
uniform float roughness;
uniform float metallic;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan; uv += 0.5;
    return uv;
}

void main() {
    // 1. 获取模型反照率（如果是读取贴图，将其从 sRGB 转换到线性空间）
    vec3 albedo = useTexture ? texture(texture_diffuse1, TexCoords).rgb : baseColor;
    albedo = pow(albedo, vec3(2.2));

    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 R = reflect(-V, N);

    // ================= PBR 环境光采样 =================
    // 使用 textureLod 根据粗糙度决定采样的模糊级别
    const float MAX_REFLECTION_LOD = 8.0; 
    
    // 镜面反射：粗糙度越高（如木头），采样的图像越模糊；粗糙度低（如金属），图像越清晰
    vec2 uv_spec = SampleSphericalMap(normalize(R));
    vec3 envSpecular = textureLod(equirectangularMap, uv_spec, roughness * MAX_REFLECTION_LOD).rgb;

    // 漫反射：直接使用最高级别的模糊图像，模拟四面八方的环境光
    vec2 uv_diff = SampleSphericalMap(N);
    vec3 envDiffuse = textureLod(equirectangularMap, uv_diff, MAX_REFLECTION_LOD).rgb;

    // ================= 菲涅尔与能量守恒 =================
    // 非金属的基础反射率通常在 0.04，金属则使用自身颜色作为基础反射
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float cosTheta = max(dot(N, V), 0.0);
    vec3 F = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    
    // 【核心修复】金属没有漫反射，强制剥夺金属的 kD 值
    kD *= 1.0 - metallic; 

    // ================= 最终光照合成 =================
    // 漫反射（严格乘以 kD）
    vec3 diffuse = kD * albedo * envDiffuse; 
    
    // 镜面反射（高光强度受粗糙度压制，避免粗糙物体泛白）
    vec3 specular = envSpecular * F * (1.0 - roughness); 

    vec3 result = diffuse + specular;

    // ================= 色调映射 Tone Mapping =================
    // 控制环境曝光度
    float exposure = 1.0; 
    result = vec3(1.0) - exp(-result * exposure);
    
    // Gamma 校正返回到显示器色彩空间
    result = pow(result, vec3(1.0 / 2.2));

    FragColor = vec4(result, 1.0);
}