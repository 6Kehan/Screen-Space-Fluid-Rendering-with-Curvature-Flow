#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;
uniform float pointRadius;
uniform float viewportHeight;

out vec3 viewSpacePos;

void main() {
    // 转换到观察空间
    vec4 posEye = view * vec4(aPos, 1.0);
    viewSpacePos = posEye.xyz;
    gl_Position = projection * posEye;
    
    // 关键：根据透视投影缩放点精灵大小
    gl_PointSize = (viewportHeight * projection[1][1] * pointRadius) / gl_Position.w;
}