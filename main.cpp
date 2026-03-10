#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Assimp 模型加载库
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

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

    float cameraSpeed = 5.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

// --- 模型数据结构 ---
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    GLuint VAO, VBO, EBO;

    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

        glBindVertexArray(0);
    }
};

std::vector<Mesh> loadGLBModel(std::string const& path) {
    std::vector<Mesh> meshes;
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
        return meshes;
    }

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* ai_mesh = scene->mMeshes[i];
        Mesh mesh;
        for (unsigned int j = 0; j < ai_mesh->mNumVertices; j++) {
            Vertex vertex;
            vertex.Position = glm::vec3(ai_mesh->mVertices[j].x, ai_mesh->mVertices[j].y, ai_mesh->mVertices[j].z);
            if (ai_mesh->HasNormals()) {
                vertex.Normal = glm::vec3(ai_mesh->mNormals[j].x, ai_mesh->mNormals[j].y, ai_mesh->mNormals[j].z);
            }
            mesh.vertices.push_back(vertex);
        }
        for (unsigned int j = 0; j < ai_mesh->mNumFaces; j++) {
            aiFace face = ai_mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++)
                mesh.indices.push_back(face.mIndices[k]);
        }
        mesh.setupMesh();
        meshes.push_back(mesh);
    }
    return meshes;
}

// --- 粒子系统 ---
struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
};

// 设定系统最大粒子容量，防止内存溢出
const int MAX_PARTICLES = 15000;

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "CS7GV3 - Fluid Emitter Demo", NULL, NULL);
    glfwMakeContextCurrent(window);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    Shader depthShader("shaders/particle.vert", "shaders/depth.frag");
    Shader thicknessShader("shaders/particle.vert", "shaders/thickness.frag");
    Shader smoothShader("shaders/screen.vert", "shaders/smooth_curvature.frag");
    Shader compositeShader("shaders/screen.vert", "shaders/composite.frag");
    Shader modelShader("shaders/model.vert", "shaders/model.frag");

    std::vector<Mesh> glassMeshes = loadGLBModel("wine_glass.glb");

    // 初始时粒子数组为空
    std::vector<Particle> particles;
    particles.reserve(MAX_PARTICLES); // 预留内存，防止扩容导致性能抖动

    GLuint particleVAO, particleVBO;
    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);
    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);

    // 【关键修改】向 GPU 预先申请 MAX_PARTICLES 大小的显存，传入 NULL 代表只开辟空间暂不填数据
    glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(Particle), NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)0);
    glEnableVertexAttribArray(0);

    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    GLuint quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint depthFBO, depthTex;
    glGenFramebuffers(1, &depthFBO);
    glGenTextures(1, &depthTex);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, SCR_WIDTH, SCR_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);

    GLuint thicknessFBO, thicknessTex;
    glGenFramebuffers(1, &thicknessFBO);
    glGenTextures(1, &thicknessTex);
    glBindTexture(GL_TEXTURE_2D, thicknessTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, thicknessFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, thicknessTex, 0);

    GLuint pingpongFBO[2], pingpongTex[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongTex);
    for (int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongTex[i], 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glEnable(GL_PROGRAM_POINT_SIZE);

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrameTime;
        lastFrameTime = currentFrame;

        processInput(window);

        // ---------------------------------------------------------
        // 【新增】粒子发射器逻辑 (按下空格键时发射)
        // ---------------------------------------------------------
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            int emitRate = 40; // 每次按下空格，这一帧吐出 40 个粒子
            for (int i = 0; i < emitRate; i++) {
                if (particles.size() < MAX_PARTICLES) {
                    Particle p;
                    // 让粒子在一个极小的口子（水龙头）处产生
                    float rx = ((rand() % 100) / 100.0f) * 0.2f - 0.1f;
                    float rz = ((rand() % 100) / 100.0f) * 0.2f - 0.1f;
                    p.position = glm::vec3(rx, 4.0f, rz); // 发射高度
                    p.velocity = glm::vec3(0.0f, -3.0f, 0.0f); // 初始发射向下的初速度
                    particles.push_back(p);
                }
            }
        }

        // ---------------------------------------------------------
        // 伪物理约束更新
        // ---------------------------------------------------------
        float sim_dt = 0.016f;
        float damping = 0.3f;
        float cupRadius = 1.2f;
        float cupBottomY = 1.0f;
        float cupHeight = 3.0f;

        for (auto& p : particles) {
            p.velocity.y -= 9.8f * sim_dt;
            p.position += p.velocity * sim_dt;

            float distFromCenter = sqrt(p.position.x * p.position.x + p.position.z * p.position.z);

            if (p.position.y < cupBottomY && distFromCenter < cupRadius) {
                p.position.y = cupBottomY;
                p.velocity.y *= -damping;
                p.velocity.x += (((rand() % 100) / 100.0f) * 2.0f - 1.0f) * 0.1f;
                p.velocity.z += (((rand() % 100) / 100.0f) * 2.0f - 1.0f) * 0.1f;
            }
            else if (p.position.y >= cupBottomY && p.position.y <= cupBottomY + cupHeight) {
                if (distFromCenter > cupRadius) {
                    glm::vec2 normal2D = glm::normalize(glm::vec2(p.position.x, p.position.z));
                    p.position.x = normal2D.x * cupRadius;
                    p.position.z = normal2D.y * cupRadius;
                    p.velocity.x *= -damping;
                    p.velocity.z *= -damping;
                }
            }
            else if (p.position.y < -5.0f) {
                // 如果粒子落出杯子且掉得太深，可以让它消失（为后续重用做准备），这里暂时简单处理让它在底部停住
                p.position.y = -5.0f;
                p.velocity.y *= -damping;
            }
        }

        // 【关键修改】只将目前"活着"的粒子数据传入显存
        if (!particles.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, particles.size() * sizeof(Particle), particles.data());
        }

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        // 如果没有粒子，跳过流体渲染管线，直接画杯子
        if (!particles.empty()) {
            // --- Fluid Pass 1: Depth ---
            glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
            glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);

            depthShader.use();
            depthShader.setMat4("view", view);
            depthShader.setMat4("projection", projection);
            depthShader.setFloat("pointRadius", 0.15f);
            depthShader.setFloat("viewportHeight", (float)SCR_HEIGHT);
            glBindVertexArray(particleVAO);
            glDrawArrays(GL_POINTS, 0, particles.size());

            // --- Fluid Pass 2: Thickness ---
            glBindFramebuffer(GL_FRAMEBUFFER, thicknessFBO);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);

            thicknessShader.use();
            thicknessShader.setMat4("view", view);
            thicknessShader.setMat4("projection", projection);
            thicknessShader.setFloat("pointRadius", 0.3f);
            thicknessShader.setFloat("viewportHeight", (float)SCR_HEIGHT);
            glBindVertexArray(particleVAO);
            glDrawArrays(GL_POINTS, 0, particles.size());
            glDisable(GL_BLEND);

            // --- Fluid Pass 3: Smooth Curvature ---
            bool horizontal = true, first_iteration = true;
            int smoothingIterations = 40;

            smoothShader.use();
            smoothShader.setMat4("projection", projection);
            smoothShader.setVec2("texelSize", glm::vec2(1.0f / SCR_WIDTH, 1.0f / SCR_HEIGHT));
            smoothShader.setFloat("dt", 0.0005f);

            glBindVertexArray(quadVAO);
            for (unsigned int i = 0; i < smoothingIterations; i++) {
                glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, first_iteration ? depthTex : pingpongTex[!horizontal]);
                smoothShader.setInt("depthMap", 0);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                horizontal = !horizontal;
                if (first_iteration) first_iteration = false;
            }

            // --- Fluid Pass 4: Composite Fluid ---
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

            // 【强制重置最终屏幕背景为纯黑】
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);

            compositeShader.use();
            compositeShader.setMat4("invProjection", glm::inverse(projection));
            compositeShader.setVec2("texelSize", glm::vec2(1.0f / SCR_WIDTH, 1.0f / SCR_HEIGHT));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, pingpongTex[!horizontal]);
            compositeShader.setInt("smoothedDepthMap", 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, thicknessTex);
            compositeShader.setInt("thicknessMap", 1);
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        else {
            // 如果连一颗粒子都没有，我们也必须把主屏幕清空为纯黑
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        // --- Pass 5: 绘制玻璃杯模型 ---
        glEnable(GL_DEPTH_TEST);

        // 调试用：依然保持模型实心不透明，待确认大小吻合后再去启用 Blend
        glDisable(GL_BLEND);

        modelShader.use();
        modelShader.setMat4("view", view);
        modelShader.setMat4("projection", projection);
        modelShader.setVec3("viewPos", cameraPos);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
        modelShader.setMat4("model", model);

        for (unsigned int i = 0; i < glassMeshes.size(); i++) {
            glBindVertexArray(glassMeshes[i].VAO);
            glDrawElements(GL_TRIANGLES, glassMeshes[i].indices.size(), GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}