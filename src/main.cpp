#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "TextureArray.hpp"
#include "Shader.hpp"
#include "Camera.hpp"
#include "ChunkSection.hpp"

int main() {
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Next-Gen Minecraft Engine", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // تفعيل اختبار العمق (Depth Test) لرسم المجسمات ثلاثية الأبعاد بدقة
    glEnable(GL_DEPTH_TEST);
    // تفعيل حجب الأوجه الخلفية لتقليل ضغط كرت الشاشة إلى النصف!
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // 1. تحميل مصفوفة التكستشر
    TextureArray textures;
    if (!textures.loadFromDirectory("res/textures/blocks")) {
        std::cerr << "Failed to load textures!\n";
        return -1;
    }

    // 2. تحميل الشيدرات
    Shader blockShader;
    if (!blockShader.loadFromFiles("res/shaders/block.vert", "res/shaders/block.frag")) {
        std::cerr << "Failed to compile shaders!\n";
        return -1;
    }

    // 3. إنشاء Chunk تجريبي بحجم 16x16x16 وملئه بالبلوكات
    ChunkSection chunk(0, 0, 0);
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            chunk.setBlock(x, 0, z, BlockType::Stone);
            chunk.setBlock(x, 1, z, BlockType::Dirt);
            chunk.setBlock(x, 2, z, BlockType::Grass);
        }
    }
    // وضع كتلة زجاج وورق شجر لتجربة الشفافية وإخفاء الأوجه
    chunk.setBlock(8, 3, 8, BlockType::Grass);
    chunk.setBlock(8, 4, 8, BlockType::OakLeaves);

    // بناء المش بـ Face Culling المضغوط
    chunk.buildMesh();

    Camera camera;

    // حلقة اللعبة (Game Loop)
    while (!glfwWindowShouldClose(window)) {
        // تنظيف الشاشة بلون السماء الكلاسيكي لماينكرافت
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        blockShader.use();
        textures.bind(0);
        blockShader.setInt("uTextureArray", 0);

        // ضبط مصفوفات الكاميرا
        glm::mat4 projection = camera.getProjectionMatrix(1280.0f / 720.0f);
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);

        // دوران خفيف للمشهد لنرى كل الأوجه وزوايا التظليل
        model = glm::rotate(model, (float)glfwGetTime() * 0.4f, glm::vec3(0.0f, 1.0f, 0.0f));

        blockShader.setMat4("uProjection", projection);
        blockShader.setMat4("uView", view);
        blockShader.setMat4("uModel", model);

        // رسم الـ Chunk
        chunk.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
