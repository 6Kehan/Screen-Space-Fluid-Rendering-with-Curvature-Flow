#version 330 core
out vec4 FragColor;

void main() {
    vec2 N = gl_PointCoord.xy * 2.0 - 1.0;
    float mag = dot(N, N);
    
    if (mag > 1.0) discard;

    float thickness = exp(-mag * 4.0); 
    
    // Output single-channel thickness value
    FragColor = vec4(thickness, 0.0, 0.0, 1.0);
}