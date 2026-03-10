#version 330 core
out vec4 FragColor;

void main() {
    vec2 N = gl_PointCoord.xy * 2.0 - 1.0;
    float mag = dot(N, N);
    
    if (mag > 1.0) discard;
    
    // 计算类似高斯核的衰减，中心最厚，边缘最薄
    float thickness = exp(-mag * 4.0); 
    
    // 输出单通道厚度值
    FragColor = vec4(thickness, 0.0, 0.0, 1.0);
}