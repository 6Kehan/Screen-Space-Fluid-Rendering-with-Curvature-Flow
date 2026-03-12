#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D depthMap;
uniform vec2 texelSize; // 1.0 / width, 1.0 / height
uniform float dt;       // 积分时间步长
uniform mat4 projection;

void main() {
    float z = texture(depthMap, TexCoords).r;
    
    // 【关键修复】检测到背景时，不要 discard，而是原样输出背景深度 1.0
    // 这样 1.0 才能传递给 composite.frag 让它正确丢弃背景以显示天空盒
    if (z >= 1.0) {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }

    // 论文公式 1, 4, 5, 6, 7, 8 的简化离散化实现
    // 1. 使用有限差分计算一阶导数 (dz/dx, dz/dy)
    float z_dx = (texture(depthMap, TexCoords + vec2(texelSize.x, 0.0)).r - 
                  texture(depthMap, TexCoords - vec2(texelSize.x, 0.0)).r) * 0.5;
    float z_dy = (texture(depthMap, TexCoords + vec2(0.0, texelSize.y)).r - 
                  texture(depthMap, TexCoords - vec2(0.0, texelSize.y)).r) * 0.5;

    // 2. 计算二阶导数 (d2z/dx2, d2z/dy2)
    float z_dx2 = texture(depthMap, TexCoords + vec2(texelSize.x, 0.0)).r - 2.0 * z + 
                  texture(depthMap, TexCoords - vec2(texelSize.x, 0.0)).r;
    float z_dy2 = texture(depthMap, TexCoords + vec2(0.0, texelSize.y)).r - 2.0 * z + 
                  texture(depthMap, TexCoords - vec2(0.0, texelSize.y)).r;

    // 3. 计算投影常数 (根据论文公式 3 简化)
    float Cx = 2.0 / (1.0 / texelSize.x * projection[0][0]);
    float Cy = 2.0 / (1.0 / texelSize.y * projection[1][1]);

    // 4. 计算均值曲率 H (论文公式 6)
    float D = Cy*Cy * z_dx*z_dx + Cx*Cx * z_dy*z_dy + Cx*Cx * Cy*Cy * z*z;

    // 为避免除以 0 导致 NaN
    if (D < 0.00001) {
        FragColor = vec4(z, 0.0, 0.0, 1.0);
        return;
    }

    float Ex = 0.5 * z_dx * (0.0 /* dD/dx 省略以简化 */) - z_dx2 * D;
    float Ey = 0.5 * z_dy * (0.0 /* dD/dy 省略以简化 */) - z_dy2 * D;

    float H = (Cy * Ex + Cx * Ey) / pow(D, 1.5);

    // 5. 欧拉积分更新深度 (论文公式 1)
    // 注意：这里的 dt 需要非常小以保证稳定性
    float new_z = z + H * dt;
    
    FragColor = vec4(new_z, 0.0, 0.0, 1.0);
}