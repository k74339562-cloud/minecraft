#ifdef __ANDROID__

#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Shader.hpp"
#include "TextureArray.hpp"
#include "ChunkSection.hpp"
#include "Camera.hpp"

#define LOG_TAG "MinecraftClone"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct Engine {
    struct android_app* app;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int32_t width = 0;
    int32_t height = 0;
    bool animating = false;

    Shader* shader = nullptr;
    TextureArray* textures = nullptr;
    ChunkSection* chunk = nullptr;
    Camera camera;
    float rotationAngle = 0.0f;
};

static int initDisplay(Engine* engine) {
    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    EGLSurface surface = eglCreateWindowSurface(display, config, engine->app->window, nullptr);
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGE("Unable to eglMakeCurrent");
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &engine->height);

    engine->display = display;
    engine->surface = surface;
    engine->context = context;

    glViewport(0, 0, engine->width, engine->height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    engine->shader = new Shader();
    engine->shader->loadFromFiles("res/shaders/block.vert", "res/shaders/block.frag");

    engine->textures = new TextureArray();
    engine->textures->loadFromDirectory("res/textures/blocks");

    engine->chunk = new ChunkSection(0, 0, 0);
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            engine->chunk->setBlock(x, 0, z, BlockType::Stone);
            engine->chunk->setBlock(x, 1, z, BlockType::Dirt);
            engine->chunk->setBlock(x, 2, z, BlockType::Grass);
        }
    }
    engine->chunk->setBlock(8, 3, 8, BlockType::Grass);
    engine->chunk->setBlock(8, 4, 8, BlockType::OakLeaves);
    engine->chunk->buildMesh();

    return 0;
}

static void drawFrame(Engine* engine) {
    if (engine->display == EGL_NO_DISPLAY) return;

    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    engine->shader->use();
    engine->textures->bind(0);
    engine->shader->setInt("uTextureArray", 0);

    float aspect = (float)engine->width / (float)engine->height;
    glm::mat4 projection = engine->camera.getProjectionMatrix(aspect);
    glm::mat4 view = engine->camera.getViewMatrix();
    glm::mat4 model = glm::mat4(1.0f);

    engine->rotationAngle += 0.015f;
    model = glm::rotate(model, engine->rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));

    engine->shader->setMat4("uProjection", projection);
    engine->shader->setMat4("uView", view);
    engine->shader->setMat4("uModel", model);

    engine->chunk->render();

    eglSwapBuffers(engine->display, engine->surface);
}

static void termDisplay(Engine* engine) {
    if (engine->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine->context != EGL_NO_CONTEXT) eglDestroyContext(engine->display, engine->context);
        if (engine->surface != EGL_NO_SURFACE) eglDestroySurface(engine->display, engine->surface);
        eglTerminate(engine->display);
    }
    engine->display = EGL_NO_DISPLAY;
    engine->context = EGL_NO_CONTEXT;
    engine->surface = EGL_NO_SURFACE;
    engine->animating = false;
}

static void handleCmd(struct android_app* app, int32_t cmd) {
    auto* engine = (Engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (engine->app->window != nullptr) {
                initDisplay(engine);
                engine->animating = true;
                drawFrame(engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            termDisplay(engine);
            break;
        case APP_CMD_GAINED_FOCUS:
            engine->animating = true;
            break;
        case APP_CMD_LOST_FOCUS:
            engine->animating = false;
            break;
    }
}

// extern "C" هي التي تجعل لغة C ترى الدالة وتمنع خطأ undefined symbol
extern "C" void android_main(struct android_app* state) {
    Engine engine{};
    state->userData = &engine;
    state->onAppCmd = handleCmd;
    engine.app = state;

    LOGI("Minecraft Android Engine initialized!");

    while (true) {
        int events;
        struct android_poll_source* source;

        while (ALooper_pollOnce(engine.animating ? 0 : -1, nullptr, &events, (void**)&source) >= 0) {
            if (source != nullptr) {
                source->process(state, source);
            }
            if (state->destroyRequested != 0) {
                termDisplay(&engine);
                return;
            }
        }

        if (engine.animating) {
            drawFrame(&engine);
        }
    }
}

#endif
