#version 330 core
uniform mat4 projection;
uniform float pointRadius;

in vec3 viewSpacePos;

void main() {
    vec3 N;
    // 将 gl_PointCoord [0, 1] 映射到 [-1, 1]
    N.xy = gl_PointCoord.xy * 2.0 - 1.0;
    N.y = -N.y; 

    float mag = dot(N.xy, N.xy);
    // 裁剪成圆形
    if (mag > 1.0) discard; 

    N.z = sqrt(1.0 - mag);
    
    // 计算真正的视空间坐标并投影计算深度
    vec4 fragPosEye = vec4(viewSpacePos + N * pointRadius, 1.0);
    vec4 clipSpacePos = projection * fragPosEye;
    
    float ndcDepth = clipSpacePos.z / clipSpacePos.w;
    gl_FragDepth = (ndcDepth + 1.0) / 2.0;
}