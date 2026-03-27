#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D smoothedDepthMap;
uniform sampler2D thicknessMap;
uniform sampler2D noiseMap;
uniform sampler2D backgroundTex; 

uniform mat4 invProjection;
uniform vec2 texelSize;

uniform vec3 fluidColor;
uniform float surfaceRippleStrength;
uniform float foamThresholdMin;
uniform float foamThresholdMax;
uniform float transparencyScale; 
uniform float refractionStrength; 

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

    // 强制限幅厚度，防止黑斑
    float thickness = texture(thicknessMap, TexCoords).r;
    thickness = min(thickness, 30.0); 

    vec3 viewPos = uvToViewSpace(TexCoords, depth);
    vec3 normal = reconstructNormal(TexCoords, depth, viewPos);
    
    // 恢复使用粒子叠加的专属 noiseMap
    float noiseVal = texture(noiseMap, TexCoords).r;
    
    // 表面波纹
    float n_dx = texture(noiseMap, TexCoords + vec2(texelSize.x, 0.0)).r - texture(noiseMap, TexCoords - vec2(texelSize.x, 0.0)).r;
    float n_dy = texture(noiseMap, TexCoords + vec2(0.0, texelSize.y)).r - texture(noiseMap, TexCoords - vec2(0.0, texelSize.y)).r;
    vec2 noiseGrad = vec2(n_dx, n_dy) * surfaceRippleStrength;
    if (length(noiseGrad) > 0.8) noiseGrad = normalize(noiseGrad) * 0.8;
    normal = normalize(normal - vec3(noiseGrad, 0.0)); 

    // 屏幕空间折射
    vec2 refractedUV = TexCoords + normal.xy * thickness * refractionStrength * 0.05;
    refractedUV = clamp(refractedUV, 0.0, 1.0);
    vec3 bgColor = texture(backgroundTex, refractedUV).rgb;

    // ==========================================
    // 【完美透明度平衡】：倍率从 0.005 恢复到 0.2
    // 这样水既通透，又有浓郁的蓝色体积感！
    // ==========================================
    vec3 absorbColor = vec3(1.0) - fluidColor;
    vec3 transmittance = exp(-absorbColor * thickness * transparencyScale * 0.2);
    
    vec3 scatterColor = fluidColor * (vec3(1.0) - transmittance);
    vec3 baseWaterColor = bgColor * transmittance + scatterColor;

    vec3 viewDir = normalize(-viewPos);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.8)); 
    vec3 halfDir = normalize(lightDir + viewDir);
    float fresnel = 0.04 + (1.0 - 0.04) * pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0);
    float spec = pow(max(dot(normal, halfDir), 0.0), 256.0); 

    vec3 finalColor = mix(baseWaterColor, vec3(0.8, 0.9, 1.0), fresnel) + vec3(0.6) * spec;

    // 恢复粒子物理泡沫，并限制最大不透明度
    float foamIntensity = smoothstep(foamThresholdMin, foamThresholdMax, noiseVal);
    foamIntensity = clamp(foamIntensity * 0.8, 0.0, 0.8); 
    
    vec3 foamColor = vec3(0.9, 0.95, 0.98); 
    finalColor = mix(finalColor, foamColor, foamIntensity);

    FragColor = vec4(finalColor, 1.0);
    gl_FragDepth = depth;
}