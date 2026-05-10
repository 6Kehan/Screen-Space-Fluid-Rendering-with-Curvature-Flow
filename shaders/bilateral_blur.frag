#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D depthMap;
uniform vec2 texelSize;
uniform mat4 invProjection;


float getViewZ(float d) {
    vec4 clipPos = vec4(TexCoords * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    vec4 viewPos = invProjection * clipPos;
    return viewPos.z / viewPos.w;
}

void main() {
    float depth = texture(depthMap, TexCoords).r;
    if (depth >= 1.0) { 
        FragColor = vec4(depth, 0.0, 0.0, 1.0); 
        return; 
    }

    float centerZ = getViewZ(depth);
    float result = 0.0;
    float weightSum = 0.0;

    float spatialSigma = 2.0;
    float rangeSigma = 0.15; 

    for(int x = -2; x <= 2; ++x) {
        for(int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            float d = texture(depthMap, TexCoords + offset).r;
            if(d < 1.0) {
                float neighborZ = getViewZ(d);
                
                // Spatial distance weight
                float spatialDist2 = float(x*x + y*y);
                float spatialWeight = exp(-spatialDist2 / (2.0 * spatialSigma * spatialSigma));
                
                // Depth difference weight
                float rangeDist = abs(neighborZ - centerZ);
                float rangeWeight = exp(-(rangeDist * rangeDist) / (2.0 * rangeSigma * rangeSigma));
                
                float w = spatialWeight * rangeWeight;
                result += d * w;
                weightSum += w;
            }
        }
    }
    FragColor = vec4(result / (weightSum + 0.0001), 0.0, 0.0, 1.0);
}