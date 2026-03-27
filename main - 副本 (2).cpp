#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Assimp 模型加载库
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// 引入 stb_image 读取 HDR 文件和模型内置贴图
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

struct Particle { glm::vec3 position; glm::vec3 velocity; };
const int MAX_PARTICLES = 15000;

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

    // 获取真实物理屏幕分辨率，适配所有高分屏/缩放屏
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

    // --- ImGui 初始化 ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // --- Shader 初始化 ---
    Shader depthShader("shaders/particle.vert", "shaders/depth.frag");
    Shader thicknessShader("shaders/particle.vert", "shaders/thickness.frag");
    Shader smoothShader("shaders/screen.vert", "shaders/smooth_curvature.frag");
    Shader noiseShader("shaders/noise.vert", "shaders/noise.frag");
    Shader compositeShader("shaders/screen.vert", "shaders/composite.frag");
    Shader skyboxShader("shaders/skybox.vert", "shaders/skybox.frag");

    // --- 粒子系统初始化 ---
    std::vector<Particle> particles;
    particles.reserve(MAX_PARTICLES);

    GLuint particleVAO, particleVBO;
    glGenVertexArrays(1, &particleVAO); glGenBuffers(1, &particleVBO);
    glBindVertexArray(particleVAO); glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(Particle), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)0);
    glEnableVertexAttribArray(0);

    // --- 屏幕四边形 ---
    float quadVertices[] = { -1.0f,  1.0f,  0.0f, 1.0f, -1.0f, -1.0f,  0.0f, 0.0f, 1.0f, -1.0f,  1.0f, 0.0f,
                             -1.0f,  1.0f,  0.0f, 1.0f,  1.0f, -1.0f,  1.0f, 0.0f, 1.0f,  1.0f,  1.0f, 1.0f };
    GLuint quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO); glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO); glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float))); glEnableVertexAttribArray(1);

    // --- Skybox ---
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

    // --- 加载 HDR 贴图 ---
    stbi_set_flip_vertically_on_load(true);
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

    // --- 所有 FBO 使用真实物理分辨率 ---
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

    // 背景捕获纹理（使用真实物理分辨率）
    GLuint backgroundTex;
    glGenTextures(1, &backgroundTex);
    glBindTexture(GL_TEXTURE_2D, backgroundTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fbWidth, fbHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ================== ImGui 变量控制 ==================
    int ui_emitRate = 40;
    float ui_pointRadius = 0.15f;
    float ui_thicknessRadius = 0.3f;
    int ui_smoothingIterations = 20;
    float ui_smoothingDt = 0.0005f;
    glm::vec3 ui_fluidColor = glm::vec3(0.1f, 0.4f, 0.8f);

    float ui_transparencyScale = 0.8f;
    float ui_refractionStrength = 0.05f;

    // 【新增】控制泡沫生成的剧烈程度
    float ui_noiseMultiplier = 0.02f;
    float ui_surfaceRipple = 0.6f;
    float ui_foamThreshold[2] = { 0.1f, 0.4f };

    bool ui_renderSkybox = true;
    bool ui_autoEmit = true;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrameTime;
        lastFrameTime = currentFrame;

        // 动态更新视口，防止窗口缩放导致的拉伸或黑边
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        processInput(window);

        // --- 启动 ImGui 帧 ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Fluid Simulation Parameters", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Press [TAB] to toggle UI / Camera Control");
        ImGui::Separator();

        ImGui::Text("Simulation");
        ImGui::Checkbox("Auto Emit Particles", &ui_autoEmit);
        ImGui::SliderInt("Emit Rate", &ui_emitRate, 1, 100);
        ImGui::Text("Particles: %d / %d", (int)particles.size(), MAX_PARTICLES);

        ImGui::Separator();
        ImGui::Text("Rendering (Curvature Flow)");
        ImGui::SliderFloat("Depth Point Radius", &ui_pointRadius, 0.05f, 0.5f);
        ImGui::SliderFloat("Thickness Radius", &ui_thicknessRadius, 0.05f, 1.0f);
        ImGui::SliderInt("Smoothing Iterations", &ui_smoothingIterations, 0, 100);
        ImGui::SliderFloat("Integration dt", &ui_smoothingDt, 0.0001f, 0.005f, "%.4f");

        ImGui::Separator();
        ImGui::Text("Fluid Optics (Refraction & Beer's Law)");
        ImGui::ColorEdit3("Fluid Color", (float*)&ui_fluidColor);
        ImGui::SliderFloat("Absorption Rate (Opacity)", &ui_transparencyScale, 0.01f, 3.0f, "%.2f");
        ImGui::SliderFloat("Refraction Strength", &ui_refractionStrength, 0.0f, 0.3f, "%.3f");

        ImGui::Separator();
        ImGui::Text("Surface Details");
        // 【新增 UI 滑块】：在这里动态控制泡沫生成
        ImGui::SliderFloat("Noise Generation", &ui_noiseMultiplier, 0.0f, 0.2f, "%.3f");
        ImGui::SliderFloat("Noise Ripple Strength", &ui_surfaceRipple, 0.0f, 2.0f);
        ImGui::SliderFloat2("Foam Thresholds", ui_foamThreshold, 0.0f, 3.0f);

        ImGui::Separator();
        ImGui::Checkbox("Render Skybox", &ui_renderSkybox);
        if (ImGui::Button("Reset Particles")) particles.clear();
        ImGui::End();

        // --- 1. 粒子发射逻辑 (瀑布与台面水流设计) ---
        if (ui_autoEmit || glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            for (int i = 0; i < ui_emitRate; i++) {
                if (particles.size() < MAX_PARTICLES) {
                    Particle p;
                    float rx = -1.8f + ((rand() % 100) / 100.0f) * 0.4f;
                    float rz = ((rand() % 100) / 100.0f) * 0.4f - 0.2f;
                    float ry = 4.0f + ((rand() % 100) / 100.0f) * 0.4f;
                    p.position = glm::vec3(rx, ry, rz);
                    p.velocity = glm::vec3(4.0f, -2.0f, 0.0f);
                    particles.push_back(p);
                }
            }
        }

        // --- 2. 轻量级物理与动态消除逻辑 ---
        float sim_dt = 0.016f;
        float damping = 0.4f;
        float tableMinX = -2.0f, tableMaxX = 1.5f, tableMinZ = -1.0f, tableMaxZ = 1.0f, tableY = 0.0f;

        for (int i = 0; i < particles.size(); ) {
            auto& p = particles[i];

            p.velocity.y -= 9.8f * sim_dt;

            // 伪表面张力：向中心靠拢，解决水流拉丝与分叉
            p.velocity.z -= p.position.z * 6.0f * sim_dt;

            p.position += p.velocity * sim_dt;

            bool onTableX = (p.position.x >= tableMinX && p.position.x <= tableMaxX);
            bool onTableZ = (p.position.z >= tableMinZ && p.position.z <= tableMaxZ);

            // 碰撞与反弹
            if (p.position.y < tableY && onTableX && onTableZ) {
                p.position.y = tableY;
                p.velocity.y *= -damping;
                p.velocity.x *= 0.90f;
                p.velocity.z *= 0.80f;
                p.velocity.x += (((rand() % 100) / 100.0f) * 2.0f - 1.0f) * 0.2f;
                p.velocity.z += (((rand() % 100) / 100.0f) * 2.0f - 1.0f) * 0.1f;
            }

            // 动态消除：掉下边缘即回收
            if (p.position.y < -3.0f) {
                particles[i] = particles.back();
                particles.pop_back();
            }
            else {
                i++;
            }
        }

        if (!particles.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, particles.size() * sizeof(Particle), particles.data());
        }

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        bool horizontal = true;

        // --- 3. 流体多通道渲染 ---
        if (!particles.empty()) {
            // Pass 3.1: Depth
            glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
            glViewport(0, 0, fbWidth, fbHeight);
            glClear(GL_DEPTH_BUFFER_BIT); glEnable(GL_DEPTH_TEST);
            depthShader.use();
            depthShader.setMat4("view", view); depthShader.setMat4("projection", projection);
            depthShader.setFloat("pointRadius", ui_pointRadius);
            depthShader.setFloat("viewportHeight", (float)fbHeight);
            glBindVertexArray(particleVAO); glDrawArrays(GL_POINTS, 0, particles.size());

            // Pass 3.2: Thickness
            glBindFramebuffer(GL_FRAMEBUFFER, thicknessFBO);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f); glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
            thicknessShader.use();
            thicknessShader.setMat4("view", view); thicknessShader.setMat4("projection", projection);
            thicknessShader.setFloat("pointRadius", ui_thicknessRadius);
            thicknessShader.setFloat("viewportHeight", (float)fbHeight);
            glBindVertexArray(particleVAO); glDrawArrays(GL_POINTS, 0, particles.size());
            glDisable(GL_BLEND);

            // Pass 3.3: Smooth Curvature
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

            // Pass 3.4: Surface Noise & Foam
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

            // 【关键连接】：将 UI 中的噪声生成倍率传给 Shader
            noiseShader.setFloat("noiseMultiplier", ui_noiseMultiplier);

            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, pingpongTex[!horizontal]);
            noiseShader.setInt("smoothedDepthMap", 0);
            glBindVertexArray(particleVAO); glDrawArrays(GL_POINTS, 0, particles.size());
            glDisable(GL_BLEND);
        }

        // ================== 屏幕合成渲染 ==================
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

        // 捕获背景供流体折射 (高分屏完美适配)
        glBindTexture(GL_TEXTURE_2D, backgroundTex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, fbWidth, fbHeight);

        // 绘制最终折射流体
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

        // --- 渲染 ImGui ---
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