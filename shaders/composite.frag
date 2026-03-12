#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D smoothedDepthMap;
uniform sampler2D thicknessMap;
uniform sampler2D noiseMap;
uniform mat4 invProjection;
uniform vec2 texelSize;

uniform vec3 fluidColor;
uniform float surfaceRippleStrength;
uniform float foamThresholdMin;
uniform float foamThresholdMax;
// 【新增】：接收 UI 传来的透明度缩放系数
uniform float transparencyScale; 

vec3 uvToViewSpace(vec2 uv, float depth) {
    float z_ndc = depth * 2.0 - 1.0;
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, z_ndc, 1.0);
    vec4 viewSpace = invProjection * clipSpace;
    return viewSpace.xyz / viewSpace.w;
}

vec3 reconstructNormal(vec2 uv, float depth, vec3 viewPos) {
    float ddx = texture(smoothedDepthMap, uv + vec2(texelSize.x, 0.0)).r;
    float ddy = texture(smoothedDepthMap, uv + vec2(0.0, texelSize.y)).r;
    vec3 viewPosRight = uvToViewSpace(uv + vec2(texelSize.x, 0.0), ddx);
    vec3 viewPosUp    = uvToViewSpace(uv + vec2(0.0, texelSize.y), ddy);
    return normalize(cross(viewPosRight - viewPos, viewPosUp - viewPos));
}

void main() {
    float depth = texture(smoothedDepthMap, TexCoords).r;
    if (depth >= 1.0) discard;

    float thickness = texture(thicknessMap, TexCoords).r;
    vec3 viewPos = uvToViewSpace(TexCoords, depth);
    vec3 normal = reconstructNormal(TexCoords, depth, viewPos);

    float noiseVal = texture(noiseMap, TexCoords).r;
    
    // 计算法线波纹
    float n_dx = texture(noiseMap, TexCoords + vec2(texelSize.x, 0.0)).r - 
                 texture(noiseMap, TexCoords - vec2(texelSize.x, 0.0)).r;
    float n_dy = texture(noiseMap, TexCoords + vec2(0.0, texelSize.y)).r - 
                 texture(noiseMap, TexCoords - vec2(0.0, texelSize.y)).r;
                 
    vec2 noiseGrad = vec2(n_dx, n_dy) * surfaceRippleStrength;
    if (length(noiseGrad) > 0.8) {
        noiseGrad = normalize(noiseGrad) * 0.8;
    }
    normal = normalize(normal - vec3(noiseGrad, 0.0)); 

    // 计算光照参数
    vec3 viewDir = normalize(-viewPos);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.8)); 
    vec3 halfDir = normalize(lightDir + viewDir);
    float fresnel = 0.04 + (1.0 - 0.04) * pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0);
    float spec = pow(max(dot(normal, halfDir), 0.0), 256.0); 

    // ==========================================================
    // 全新混合逻辑：真实的 Alpha 输出
    // ==========================================================
    
    // 1. 根据厚度和UI参数，计算基础水体的阻光度（利用指数衰减，防止越界）
    float fluidAlpha = 1.0 - exp(-thickness * transparencyScale);
    
    // 2. 计算流体自身颜色 + 反射光
    vec3 litColor = fluidColor * (1.0 - fresnel) + vec3(1.0) * fresnel + vec3(0.6) * spec;
    
    // 3. 计算并混合泡沫层
    float foamIntensity = smoothstep(foamThresholdMin, foamThresholdMax, noiseVal);
    foamIntensity = clamp(foamIntensity * 0.8, 0.0, 0.8); 
    vec3 foamColor = vec3(0.9, 0.95, 0.98); 
    vec3 finalColor = mix(litColor, foamColor, foamIntensity);
    
    // 4. 关键：确保边缘反射（Fresnel）和表面泡沫始终是不透明的，无论水有多浅！
    float finalAlpha = max(fluidAlpha, foamIntensity); 
    finalAlpha = max(finalAlpha, fresnel); 
    
    // 最终输出 RGBA
    FragColor = vec4(finalColor, finalAlpha);
    gl_FragDepth = depth;
}