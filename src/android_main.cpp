#ifdef __ANDROID__

#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>

#include "Shader.hpp"
#include "ChunkSection.hpp"
#include "Camera.hpp"
#include "stb_image.h"

#define LOG_TAG "MinecraftClone"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// شيدر الرؤوس لفك ضغط الفيرتكس 32-bit
const char* VERTEX_SHADER_SRC = R"(#version 300 es
layout (location = 0) in uint aPackedData;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vTexCoord;
flat out int vLayer;
out float vLight;

const vec2 UV_CORNERS[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

const float FACE_LIGHT[6] = float[6](
    1.0,  // UP
    0.5,  // DOWN
    0.8,  // NORTH
    0.8,  // SOUTH
    0.6,  // WEST
    0.6   // EAST
);

void main() {
    uint x       = aPackedData & 31u;
    uint y       = (aPackedData >> 5u) & 31u;
    uint z       = (aPackedData >> 10u) & 31u;
    uint faceDir = (aPackedData >> 15u) & 7u;
    uint ao      = (aPackedData >> 18u) & 3u;
    uint layer   = (aPackedData >> 20u) & 1023u;
    uint uvIndex = (aPackedData >> 30u) & 3u;

    vec3 localPos = vec3(float(x), float(y), float(z));

    vTexCoord = UV_CORNERS[uvIndex];
    vLayer = int(layer);
    vLight = FACE_LIGHT[faceDir];

    gl_Position = uProjection * uView * uModel * vec4(localPos, 1.0);
}
)";

// شيدر البكسلات: قراءة صور التكستشر الحقيقية بدقة البكسل الحادة!
const char* FRAGMENT_SHADER_SRC = R"(#version 300 es
precision mediump float;
precision mediump sampler2DArray;

in vec2 vTexCoord;
flat in int vLayer;
in float vLight;

uniform sampler2DArray uTextureArray;

out vec4 FragColor;

void main() {
    // قراءة البكسل من مصفوفة التكستشر بالطبقة المحددة
    vec4 texColor = texture(uTextureArray, vec3(vTexCoord, float(vLayer)));

    // إذا كان البكسل شفافاً الغِه (لأوراق الشجر)
    if (texColor.a < 0.5) {
        discard;
    }

    // دمج لون التكستشر الأصلي مع تظليل ماينكرافت
    FragColor = vec4(texColor.rgb * vLight, texColor.a);
}
)";

struct Engine {
    struct android_app* app;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int32_t width = 0;
    int32_t height = 0;
    bool animating = false;

    GLuint shaderProgram = 0;
    GLuint textureArrayID = 0;
    ChunkSection* chunk = nullptr;
    Camera camera;
    float rotationAngle = 0.0f;
};

// قراءة ملفات الصور من حزمة APK وتكوين مصفوفة GL_TEXTURE_2D_ARRAY
static GLuint loadTexturesFromApk(AAssetManager* mgr) {
    std::vector<std::string> textureNames = {
        "grass_block_top.png",   // الطبقة 0
        "grass_block_side.png",  // الطبقة 1
        "dirt.png",              // الطبقة 2
        "stone.png",             // الطبقة 3
        "oak_leaves.png"         // الطبقة 4
    };

    GLuint texID = 0;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texID);

    int layerCount = static_cast<int>(textureNames.size());
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 16, 16, layerCount, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    stbi_set_flip_vertically_on_load(1);

    for (int i = 0; i < layerCount; ++i) {
        std::string filename = textureNames[i];
        
        // محاولة البحث عن الصورة في مسارات الأصول
        AAsset* asset = AAssetManager_open(mgr, ("textures/blocks/" + filename).c_str(), AASSET_MODE_BUFFER);
        if (!asset) {
            asset = AAssetManager_open(mgr, ("res/textures/blocks/" + filename).c_str(), AASSET_MODE_BUFFER);
        }

        if (asset) {
            size_t size = AAsset_getLength(asset);
            const void* buffer = AAsset_getBuffer(asset);

            int w, h, channels;
            unsigned char* data = stbi_load_from_memory((const unsigned char*)buffer, (int)size, &w, &h, &channels, 4);

            if (data) {
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
                stbi_image_free(data);
                LOGI("Loaded texture %s to Layer %d", filename.c_str(), i);
            }
            AAsset_close(asset);
        } else {
            LOGE("Could not find asset: %s (generating fallback)", filename.c_str());
            // نمط احتياطي بلون البلوكة إذا لم يجد الصورة
            std::vector<uint32_t> fallback(16 * 16, (i == 0 ? 0xFF55AA55 : (i == 1 ? 0xFF336688 : 0xFF224466)));
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, 16, 16, 1, GL_RGBA, GL_UNSIGNED_BYTE, fallback.data());
        }
    }

    // فلترة البكسلات الحادة (مثل ماينكرافت تماماً وبدون أي تغبيش)
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    return texID;
}

static GLuint compileShaderSrc(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    return shader;
}

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

    // تجميع الشيدر
    GLuint vs = compileShaderSrc(GL_VERTEX_SHADER, VERTEX_SHADER_SRC);
    GLuint fs = compileShaderSrc(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SRC);
    engine->shaderProgram = glCreateProgram();
    glAttachShader(engine->shaderProgram, vs);
    glAttachShader(engine->shaderProgram, fs);
    glLinkProgram(engine->shaderProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // تحميل صور التكستشر الحقيقية الخاصة بك
    engine->textureArrayID = loadTexturesFromApk(engine->app->activity->assetManager);

    // بناء الـ Chunk
    engine->chunk = new ChunkSection(0, 0, 0);
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            engine->chunk->setBlock(x, 0, z, BlockType::Stone);
            engine->chunk->setBlock(x, 1, z, BlockType::Dirt);
            engine->chunk->setBlock(x, 2, z, BlockType::Grass);
        }
    }
    // إضافة مكعبات عشب وشجر في المنتصف
    engine->chunk->setBlock(8, 3, 8, BlockType::Grass);
    engine->chunk->setBlock(8, 4, 8, BlockType::OakLeaves);
    engine->chunk->setBlock(8, 5, 8, BlockType::OakLeaves);

    engine->chunk->buildMesh();

    engine->camera.position = glm::vec3(8.0f, 14.0f, 22.0f);
    engine->camera.front = glm::normalize(glm::vec3(8.0f, 2.0f, 8.0f) - engine->camera.position);

    return 0;
}

static void drawFrame(Engine* engine) {
    if (engine->display == EGL_NO_DISPLAY) return;

    // لون سماء ماينكرافت الصافية
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(engine->shaderProgram);

    // تفعيل مصفوفة التكستشر وربطها بالشيدر
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, engine->textureArrayID);
    GLint uTexLoc = glGetUniformLocation(engine->shaderProgram, "uTextureArray");
    glUniform1i(uTexLoc, 0);

    float aspect = (float)engine->width / (float)engine->height;
    glm::mat4 projection = engine->camera.getProjectionMatrix(aspect);
    glm::mat4 view = engine->camera.getViewMatrix();
    glm::mat4 model = glm::mat4(1.0f);

    // تدوير المشهد بنعومة حول المركز
    engine->rotationAngle += 0.012f;
    model = glm::translate(model, glm::vec3(8.0f, 0.0f, 8.0f));
    model = glm::rotate(model, engine->rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::translate(model, glm::vec3(-8.0f, 0.0f, -8.0f));

    GLint uProjLoc = glGetUniformLocation(engine->shaderProgram, "uProjection");
    GLint uViewLoc = glGetUniformLocation(engine->shaderProgram, "uView");
    GLint uModelLoc = glGetUniformLocation(engine->shaderProgram, "uModel");

    glUniformMatrix4fv(uProjLoc, 1, GL_FALSE, &projection[0][0]);
    glUniformMatrix4fv(uViewLoc, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, &model[0][0]);

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

extern "C" void android_main(struct android_app* state) {
    Engine engine{};
    state->userData = &engine;
    state->onAppCmd = handleCmd;
    engine.app = state;

    LOGI("Minecraft Android Engine started!");

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
