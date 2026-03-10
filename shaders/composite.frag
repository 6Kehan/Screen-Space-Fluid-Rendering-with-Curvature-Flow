#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D smoothedDepthMap;
uniform sampler2D thicknessMap;
uniform mat4 invProjection;
uniform vec2 texelSize;

// 从屏幕 NDC 坐标反推视图空间坐标 (View Space)
vec3 uvToViewSpace(vec2 uv, float depth) {
    float z_ndc = depth * 2.0 - 1.0;
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, z_ndc, 1.0);
    vec4 viewSpace = invProjection * clipSpace;
    return viewSpace.xyz / viewSpace.w;
}

// 屏幕空间有限差分计算法线
vec3 reconstructNormal(vec2 uv, float depth, vec3 viewPos) {
    float ddx = texture(smoothedDepthMap, uv + vec2(texelSize.x, 0.0)).r;
    float ddy = texture(smoothedDepthMap, uv + vec2(0.0, texelSize.y)).r;
    
    vec3 viewPosRight = uvToViewSpace(uv + vec2(texelSize.x, 0.0), ddx);
    vec3 viewPosUp    = uvToViewSpace(uv + vec2(0.0, texelSize.y), ddy);

    vec3 ddx_pos = viewPosRight - viewPos;
    vec3 ddy_pos = viewPosUp - viewPos;

    return normalize(cross(ddx_pos, ddy_pos));
}

void main() {
    float depth = texture(smoothedDepthMap, TexCoords).r;
    // 如果是背景深度，则直接丢弃
    if (depth >= 1.0) discard;

    float thickness = texture(thicknessMap, TexCoords).r;
    vec3 viewPos = uvToViewSpace(TexCoords, depth);
    vec3 normal = reconstructNormal(TexCoords, depth, viewPos);

    // 视角与光照向量
    vec3 viewDir = normalize(-viewPos);
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 halfDir = normalize(lightDir + viewDir);

    // 论文 3.5 节的光照模型：菲涅耳方程
    float fresnel = 0.04 + (1.0 - 0.04) * pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0);
    float spec = pow(max(dot(normal, halfDir), 0.0), 128.0);

    // 颜色随厚度衰减
    vec3 fluidColor = vec3(0.1, 0.4, 0.8);
    // 流体越厚，吸收背景越多，颜色越深
    vec3 refractionColor = mix(vec3(1.0), fluidColor, clamp(thickness * 0.5, 0.0, 1.0)); 

    vec3 finalColor = refractionColor * (1.0 - fresnel) + vec3(1.0) * fresnel + vec3(1.0) * spec;
    
    FragColor = vec4(finalColor, 1.0);
}