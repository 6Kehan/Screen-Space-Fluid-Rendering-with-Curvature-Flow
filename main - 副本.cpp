#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Assimp 模型加载库
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// 引入 stb_image 读取 HDR 文件和贴图
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// --- 引入 ImGui ---
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include <vector>
#include <random>

#include "Shader.h"

const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

// --- 摄像机系统全局变量 ---
glm::vec3 cameraPos = glm::vec3(0.0f, 4.0f, 8.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.3f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

bool firstMouse = true;
float yaw = -90.0f;
float pitch = -20.0f;
float lastX = SCR_WIDTH / 2.0;
float lastY = SCR_HEIGHT / 2.0;

float deltaTime = 0.0f;
float lastFrameTime = 0.0f;

// 鼠标控制状态
bool cursorEnabled = false;
bool tabKeyPressed = false;

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (cursorEnabled) {
        firstMouse = true;
        return;
    }
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos; lastY = ypos;
    float sensitivity = 0.1f;
    xoffset *= sensitivity; yoffset *= sensitivity;
    yaw += xoffset; pitch += yoffset;
    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
        if (!tabKeyPressed) {
            cursorEnabled = !cursorEnabled;
            glfwSetInputMode(window, GLFW_CURSOR, cursorEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            tabKeyPressed = true;
        }
    }
    else { tabKeyPressed = false; }
    float cameraSpeed = 5.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

// ==========================================
// --- 专门用于加载独立 PBR 贴图的函数 ---
// ==========================================
unsigned int loadTexture(char const* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format = GL_RGB;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    }
    else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }
    return textureID;
}

struct Particle { glm::vec3 position; glm::vec3 velocity; };
const int MAX_PARTICLES = 15000;

struct Vertex { glm::vec3 Position; glm::vec3 Normal; glm::vec2 TexCoords; };
struct Mesh {
    std::vector<Vertex> vertices; std::vector<unsigned int> indices; GLuint VAO, VBO, EBO;
    void setupMesh() {
        glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO); glGenBuffers(1, &EBO);
        glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
        glBindVertexArray(0);
    }
};

// --- 通用模型加载 (FBX/GLTF/OBJ) ---
std::vector<Mesh> loadModel(std::string const& path) {
    std::vector<Mesh> meshes; Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_OptimizeMeshes);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return meshes;
    }

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* ai_mesh = scene->mMeshes[i]; Mesh mesh;
        for (unsigned int j = 0; j < ai_mesh->mNumVertices; j++) {
            Vertex vertex;
            vertex.Position = glm::vec3(ai_mesh->mVertices[j].x, ai_mesh->mVertices[j].y, ai_mesh->mVertices[j].z);
            if (ai_mesh->HasNormals()) vertex.Normal = glm::vec3(ai_mesh->mNormals[j].x, ai_mesh->mNormals[j].y, ai_mesh->mNormals[j].z);
            else vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);

            if (ai_mesh->mTextureCoords[0]) vertex.TexCoords = glm::vec2(ai_mesh->mTextureCoords[0][j].x, ai_mesh->mTextureCoords[0][j].y);
            else vertex.TexCoords = glm::vec2(0.0f, 0.0f);

            mesh.vertices.push_back(vertex);
        }
        for (unsigned int j = 0; j < ai_mesh->mNumFaces; j++) {
            aiFace face = ai_mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++) mesh.indices.push_back(face.mIndices[k]);
        }
        mesh.setupMesh(); meshes.push_back(mesh);
    }
    return meshes;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Screen Space Fluid Rendering", NULL, NULL);
    glfwMakeContextCurrent(window);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    Shader depthShader("shaders/particle.vert", "shaders/depth.frag");
    Shader thicknessShader("shaders/particle.vert", "shaders/thickness.frag");
    Shader smoothShader("shaders/screen.vert", "shaders/smooth_curvature.frag");
    Shader noiseShader("shaders/noise.vert", "shaders/noise.frag");
    Shader compositeShader("shaders/screen.vert", "shaders/composite.frag");
    Shader skyboxShader("shaders/skybox.vert", "shaders/skybox.frag");
    Shader modelShader("shaders/model.vert", "shaders/model.frag");

    std::vector<Particle> particles;
    particles.reserve(MAX_PARTICLES);

    GLuint particleVAO, particleVBO;
    glGenVertexArrays(1, &particleVAO); glGenBuffers(1, &particleVBO);
    glBindVertexArray(particleVAO); glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(Particle), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)0);
    glEnableVertexAttribArray(0);

    float quadVertices[] = { -1.0f,  1.0f,  0.0f, 1.0f, -1.0f, -1.0f,  0.0f, 0.0f, 1.0f, -1.0f,  1.0f, 0.0f,
                             -1.0f,  1.0f,  0.0f, 1.0f,  1.0f, -1.0f,  1.0f, 0.0f, 1.0f,  1.0f,  1.0f, 1.0f };
    GLuint quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO); glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO); glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float))); glEnableVertexAttribArray(1);

    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
    };
    GLuint skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO); glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO); glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // ==========================================
    // --- 加载 FBX 模型和专属 PBR 贴图 ---
    // ==========================================
    std::vector<Mesh> pipeMeshes = loadModel("waters-pipe.fbx");

    // 手动加载四张关键物理贴图
    stbi_set_flip_vertically_on_load(false); // 通常模型贴图不需要像 HDR 那样垂直翻转
    unsigned int albedoMap = loadTexture("textures/T_DefaultMaterial_BaseColor.png");
    unsigned int metallicMap = loadTexture("textures/T_DefaultMaterial_Metallic.png");
    unsigned int roughnessMap = loadTexture("textures/T_DefaultMaterial_Roughness.png");
    unsigned int aoMap = loadTexture("textures/T_DefaultMaterial_AO.png");
    stbi_set_flip_vertically_on_load(true); // 为 HDR 贴图恢复翻转

    int width, height, nrComponents;
    float* data = stbi_loadf("story_studio_01_4k.hdr", &width, &height, &nrComponents, 0);
    unsigned int hdrTexture = 0;
    if (data) {
        glGenTextures(1, &hdrTexture); glBindTexture(GL_TEXTURE_2D, hdrTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    }

    auto createFBO = [&](GLuint& fbo, GLuint& tex, GLenum internalFormat, GLenum format, GLenum type, bool isDepth = false) {
        glGenFramebuffers(1, &fbo); glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, fbWidth, fbHeight, 0, format, type, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, isDepth ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, isDepth ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, isDepth ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    };

    GLuint depthFBO, depthTex, thicknessFBO, thicknessTex, noiseFBO, noiseTex;
    createFBO(depthFBO, depthTex, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, true);
    createFBO(thicknessFBO, thicknessTex, GL_R32F, GL_RED, GL_FLOAT);
    createFBO(noiseFBO, noiseTex, GL_R32F, GL_RED, GL_FLOAT);

    GLuint pingpongFBO[2], pingpongTex[2];
    for (int i = 0; i < 2; i++) createFBO(pingpongFBO[i], pingpongTex[i], GL_R32F, GL_RED, GL_FLOAT);

    GLuint backgroundTex;
    glGenTextures(1, &backgroundTex);
    glBindTexture(GL_TEXTURE_2D, backgroundTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fbWidth, fbHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ================== ImGui 变量 ==================
    int ui_emitRate = 40;
    float ui_pointRadius = 0.15f;
    float ui_thicknessRadius = 0.3f;
    int ui_smoothingIterations = 20;
    float ui_smoothingDt = 0.0005f;
    glm::vec3 ui_fluidColor = glm::vec3(0.1f, 0.4f, 0.8f);
    float ui_transparencyScale = 0.8f;
    float ui_refractionStrength = 0.05f;
    float ui_noiseMultiplier = 0.02f;
    float ui_surfaceRipple = 0.6f;
    float ui_foamThreshold[2] = { 0.1f, 0.4f };
    bool ui_renderSkybox = true;
    bool ui_autoEmit = true;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrameTime;
        lastFrameTime = currentFrame;

        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        processInput(window);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Fluid Simulation Parameters", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Separator();
        ImGui::Text("Simulation");
        ImGui::Checkbox("Auto Emit Particles", &ui_autoEmit);
        ImGui::SliderInt("Emit Rate", &ui_emitRate, 1, 100);
        ImGui::Separator();
        ImGui::Text("Rendering (Curvature Flow)");
        ImGui::SliderFloat("Depth Point Radius", &ui_pointRadius, 0.05f, 0.5f);
        ImGui::SliderFloat("Thickness Radius", &ui_thicknessRadius, 0.05f, 1.0f);
        ImGui::SliderInt("Smoothing Iterations", &ui_smoothingIterations, 0, 100);
        ImGui::Separator();
        ImGui::Text("Fluid Optics (Refraction)");
        ImGui::ColorEdit3("Fluid Color", (float*)&ui_fluidColor);
        ImGui::SliderFloat("Opacity", &ui_transparencyScale, 0.01f, 3.0f, "%.2f");
        ImGui::SliderFloat("Refraction", &ui_refractionStrength, 0.0f, 0.3f, "%.3f");
        ImGui::Separator();
        ImGui::Text("Surface Details");
        ImGui::SliderFloat("Noise Generation", &ui_noiseMultiplier, 0.0f, 0.2f, "%.3f");
        ImGui::SliderFloat("Noise Ripple Strength", &ui_surfaceRipple, 0.0f, 2.0f);
        ImGui::SliderFloat2("Foam Thresholds", ui_foamThreshold, 0.0f, 3.0f);
        ImGui::Separator();
        ImGui::Checkbox("Render Skybox", &ui_renderSkybox);
        if (ImGui::Button("Reset Particles")) particles.clear();
        ImGui::End();

        // --- 1. 粒子发射逻辑 ---
        if (ui_autoEmit || glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            for (int i = 0; i < ui_emitRate; i++) {
                if (particles.size() < MAX_PARTICLES) {
                    Particle p;
                    float rx = -1.8f + ((rand() % 100) / 100.0f) * 0.1f - 0.05f;
                    float rz = ((rand() % 100) / 100.0f) * 0.1f - 0.05f;
                    float ry = 4.0f + ((rand() % 100) / 100.0f) * 0.1f;
                    p.position = glm::vec3(rx, ry, rz);
                    p.velocity = glm::vec3(4.0f, -2.0f, 0.0f);
                    particles.push_back(p);
                }
            }
        }

        // --- 2. 轻量级物理逻辑 ---
        float sim_dt = 0.016f;
        float damping = 0.4f;
        float tableMinX = -2.0f, tableMaxX = 1.5f, tableMinZ = -1.0f, tableMaxZ = 1.0f, tableY = 0.0f;

        for (int i = 0; i < particles.size(); ) {
            auto& p = particles[i];

            p.velocity.y -= 9.8f * sim_dt;

            // 表面张力防止分叉
            p.velocity.z -= p.position.z * 6.0f * sim_dt;

            p.position += p.velocity * sim_dt;

            bool onTableX = (p.position.x >= tableMinX && p.position.x <= tableMaxX);
            bool onTableZ = (p.position.z >= tableMinZ && p.position.z <= tableMaxZ);

            if (p.position.y < tableY && onTableX && onTableZ) {
                p.position.y = tableY;
                p.velocity.y *= -damping;
                p.velocity.x *= 0.90f;
                p.velocity.z *= 0.80f;
                p.velocity.x += (((rand() % 100) / 100.0f) * 2.0f - 1.0f) * 0.15f;
                p.velocity.z += (((rand() % 100) / 100.0f) * 2.0f - 1.0f) * 0.05f;
            }

            if (p.position.y < -3.0f) {
                particles[i] = particles.back();
                particles.pop_back();
            }
            else { i++; }
        }

        if (!particles.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, particles.size() * sizeof(Particle), particles.data());
        }

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        bool horizontal = true;

        if (!particles.empty()) {
            glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
            glViewport(0, 0, fbWidth, fbHeight);
            glClear(GL_DEPTH_BUFFER_BIT); glEnable(GL_DEPTH_TEST);
            depthShader.use();
            depthShader.setMat4("view", view); depthShader.setMat4("projection", projection);
            depthShader.setFloat("pointRadius", ui_pointRadius);
            depthShader.setFloat("viewportHeight", (float)fbHeight);
            glBindVertexArray(particleVAO); glDrawArrays(GL_POINTS, 0, particles.size());

            glBindFramebuffer(GL_FRAMEBUFFER, thicknessFBO);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f); glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
            thicknessShader.use();
            thicknessShader.setMat4("view", view); thicknessShader.setMat4("projection", projection);
            thicknessShader.setFloat("pointRadius", ui_thicknessRadius);
            thicknessShader.setFloat("viewportHeight", (float)fbHeight);
            glBindVertexArray(particleVAO); glDrawArrays(GL_POINTS, 0, particles.size());
            glDisable(GL_BLEND);

            bool first_iteration = true;
            smoothShader.use();
            smoothShader.setMat4("projection", projection);
            smoothShader.setVec2("texelSize", glm::vec2(1.0f / fbWidth, 1.0f / fbHeight));
            smoothShader.setFloat("dt", ui_smoothingDt);

            glBindVertexArray(quadVAO);
            for (int i = 0; i < ui_smoothingIterations; i++) {
                glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, first_iteration ? depthTex : pingpongTex[!horizontal]);
                smoothShader.setInt("depthMap", 0);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                horizontal = !horizontal;
                if (first_iteration) first_iteration = false;
            }

            glBindFramebuffer(GL_FRAMEBUFFER, noiseFBO);
            glViewport(0, 0, fbWidth, fbHeight);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f); glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
            noiseShader.use();
            noiseShader.setMat4("view", view); noiseShader.setMat4("projection", projection);
            noiseShader.setMat4("invProjection", glm::inverse(projection));
            noiseShader.setFloat("pointRadius", ui_pointRadius);
            noiseShader.setFloat("viewportHeight", (float)fbHeight);
            noiseShader.setVec2("texelSize", glm::vec2(1.0f / fbWidth, 1.0f / fbHeight));
            noiseShader.setFloat("noiseMultiplier", ui_noiseMultiplier);

            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, pingpongTex[!horizontal]);
            noiseShader.setInt("smoothedDepthMap", 0);
            glBindVertexArray(particleVAO); glDrawArrays(GL_POINTS, 0, particles.size());
            glDisable(GL_BLEND);
        }

        // ================== 最终合成渲染 ==================
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (ui_renderSkybox) {
            glDisable(GL_DEPTH_TEST);
            skyboxShader.use();
            glm::mat4 view_skybox = glm::mat4(glm::mat3(view));
            skyboxShader.setMat4("view", view_skybox); skyboxShader.setMat4("projection", projection);
            skyboxShader.setInt("mode", 0); skyboxShader.setInt("equirectangularMap", 0);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hdrTexture);
            glBindVertexArray(skyboxVAO); glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // --- 绘制实体贴图模型 ---
        if (!pipeMeshes.empty()) {
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);

            modelShader.use();
            modelShader.setMat4("view", view);
            modelShader.setMat4("projection", projection);

            glm::mat4 model = glm::mat4(1.0f);
            //model = glm::translate(model, glm::vec3(-1.85f, 4.02f, -0.2f));
            model = glm::translate(model, glm::vec3(-2.7f, 4.5f, 0.0f));
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));

            modelShader.setMat4("model", model);
            modelShader.setVec3("lightDir", glm::normalize(glm::vec3(0.5, 1.0, 0.8)));
            modelShader.setVec3("viewPos", cameraPos);

            // 【注入贴图】：为模型绑定四张基于物理的精美贴图
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, albedoMap); modelShader.setInt("albedoMap", 0);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, metallicMap); modelShader.setInt("metallicMap", 1);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, roughnessMap); modelShader.setInt("roughnessMap", 2);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, aoMap); modelShader.setInt("aoMap", 3);

            for (auto& mesh : pipeMeshes) {
                glBindVertexArray(mesh.VAO);
                glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
                glBindVertexArray(0);
            }
        }

        // 捕获背景供流体物理折射
        glBindTexture(GL_TEXTURE_2D, backgroundTex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, fbWidth, fbHeight);

        if (!particles.empty()) {
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);

            compositeShader.use();
            compositeShader.setMat4("invProjection", glm::inverse(projection));
            compositeShader.setVec2("texelSize", glm::vec2(1.0f / fbWidth, 1.0f / fbHeight));

            compositeShader.setVec3("fluidColor", ui_fluidColor);
            compositeShader.setFloat("surfaceRippleStrength", ui_surfaceRipple);
            compositeShader.setFloat("foamThresholdMin", ui_foamThreshold[0]);
            compositeShader.setFloat("foamThresholdMax", ui_foamThreshold[1]);
            compositeShader.setFloat("transparencyScale", ui_transparencyScale);
            compositeShader.setFloat("refractionStrength", ui_refractionStrength);

            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, pingpongTex[!horizontal]);
            compositeShader.setInt("smoothedDepthMap", 0);

            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, thicknessTex);
            compositeShader.setInt("thicknessMap", 1);

            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, noiseTex);
            compositeShader.setInt("noiseMap", 2);

            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, backgroundTex);
            compositeShader.setInt("backgroundTex", 3);

            glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}