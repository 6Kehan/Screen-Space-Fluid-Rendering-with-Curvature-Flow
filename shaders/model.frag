#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;

uniform vec3 viewPos;

void main() {
    // 1. 基础陶瓷色
    vec3 objectColor = vec3(0.8, 0.8, 0.8); 
    vec3 lightPos = vec3(5.0, 10.0, 5.0);
    vec3 lightColor = vec3(1.0, 1.0, 1.0);

    // 2. 增强环境光 (Ambient) - 调高这个值，物体在暗处也会变亮
    float ambientStrength = 0.5; 
    vec3 ambient = ambientStrength * lightColor;

    // 3. 漫反射 (Diffuse)
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // 4. 镜面高光 (Specular)
    float specularStrength = 0.8;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    // 5. 边缘光 (Rim Light) - 专门用于在黑背景下勾勒轮廓
    float rimPower = 3.0;
    float rimIntensity = pow(1.0 - max(dot(norm, viewDir), 0.0), rimPower);
    vec3 rim = rimIntensity * vec3(1.0, 1.0, 1.0) * 0.5;

    vec3 result = (ambient + diffuse + specular + rim) * objectColor;
    FragColor = vec4(result, 1.0);
}