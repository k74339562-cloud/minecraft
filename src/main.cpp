#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "TextureArray.hpp"

int main() {
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Voxel Engine", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // تجربة قراءة الصور
    TextureArray textures;
    if (textures.loadFromDirectory("res/textures/blocks")) {
        std::cout << "\n✅ SUCCESS: All textures loaded into GL_TEXTURE_2D_ARRAY successfully!\n";
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
