#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D depthMap;
uniform vec2 texelSize;

void main() {
    float depth = texture(depthMap, TexCoords).r;
    // Skip blurring if it's the background
    if (depth >= 1.0) { 
        FragColor = vec4(depth, 0.0, 0.0, 1.0); 
        return; 
    }

    float result = 0.0;
    float weightSum = 0.0;
    // 5x5 Gaussian kernel weights
    float weights[5] = float[](0.06136, 0.24477, 0.38774, 0.24477, 0.06136);

    for(int x = -2; x <= 2; ++x) {
        for(int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            float d = texture(depthMap, TexCoords + offset).r;
            // Only blur pixels inside the fluid to prevent mixing with background depth
            if(d < 1.0) { 
                float w = weights[x+2] * weights[y+2];
                result += d * w;
                weightSum += w;
            }
        }
    }
    FragColor = vec4(result / (weightSum + 0.0001), 0.0, 0.0, 1.0);
}