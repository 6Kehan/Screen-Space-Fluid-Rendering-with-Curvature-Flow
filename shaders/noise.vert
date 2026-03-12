#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 ViewPos;
out vec2 NoiseOffset;

uniform mat4 view;
uniform mat4 projection;
uniform float pointRadius;
uniform float viewportHeight;

void main() {
    vec4 viewPos = view * vec4(aPos, 1.0);
    ViewPos = viewPos.xyz;
    
    // 使用 gl_VertexID 作为伪随机数种子，为每个粒子生成独一无二的固定噪声偏移
    NoiseOffset = vec2(
        fract(sin(float(gl_VertexID) * 12.9898) * 43758.5453),
        fract(cos(float(gl_VertexID) * 78.233) * 43758.5453)
    ) * 100.0; 

    gl_Position = projection * viewPos;
    gl_PointSize = (viewportHeight * projection[1][1] * pointRadius) / -viewPos.z;
}