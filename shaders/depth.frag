#version 330 core
uniform mat4 projection;
uniform float pointRadius;

in vec3 viewSpacePos;

void main() {
    vec3 N;
    // Map gl_PointCoord [0, 1] to [-1, 1]
    N.xy = gl_PointCoord.xy * 2.0 - 1.0;
    N.y = -N.y; 

    float mag = dot(N.xy, N.xy);
    // Clip to circle shape
    if (mag > 1.0) discard; 

    N.z = sqrt(1.0 - mag);
    
    // Calculate real view-space position and project to compute depth
    vec4 fragPosEye = vec4(viewSpacePos + N * pointRadius, 1.0);
    vec4 clipSpacePos = projection * fragPosEye;
    
    float ndcDepth = clipSpacePos.z / clipSpacePos.w;
    gl_FragDepth = (ndcDepth + 1.0) / 2.0;
}