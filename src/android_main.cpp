#ifdef __ANDROID__

#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/input.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include <cmath>

#include "Shader.hpp"
#include "ChunkSection.hpp"
#include "Camera.hpp"
#include "stb_image.h"

#define LOG_TAG "MinecraftClone"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static float hash2D(int x, int z) {
    int n = x + z * 57;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

static float smoothNoise(float x, float z) {
    int ix = (int)std::floor(x);
    int iz = (int)std::floor(z);
    float fx = x - ix;
    float fz = z - iz;

    fx = fx * fx * (3.0f - 2.0f * fx);
    fz = fz * fz * (3.0f - 2.0f * fz);

    float s00 = hash2D(ix, iz);
    float s10 = hash2D(ix + 1, iz);
    float s01 = hash2D(ix, iz + 1);
    float s11 = hash2D(ix + 1, iz + 1);

    float x0 = s00 * (1.0f - fx) + s10 * fx;
    float x1 = s01 * (1.0f - fx) + s11 * fx;

    return x0 * (1.0f - fz) + x1 * fz;
}

static int getTerrainHeight(int x, int z) {
    float noise = 0.0f;
    noise += smoothNoise(x * 0.12f, z * 0.12f) * 3.5f;
    noise += smoothNoise(x * 0.25f, z * 0.25f) * 1.5f;
    int h = 4 + (int)noise;
    if (h < 2) h = 2;
    if (h > 9) h = 9;
    return h;
}

// -------------------------------------------------------------
// شيدرات 3D للعالم والغيوم
// -------------------------------------------------------------
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
    1.0, 0.5, 0.8, 0.8, 0.6, 0.6
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

const char* FRAGMENT_SHADER_SRC = R"(#version 300 es
precision mediump float;
precision mediump sampler2DArray;

in vec2 vTexCoord;
flat in int vLayer;
in float vLight;

uniform sampler2DArray uTextureArray;

out vec4 FragColor;

void main() {
    vec4 texColor = texture(uTextureArray, vec3(vTexCoord, float(vLayer)));
    if (texColor.a < 0.5) discard;
    FragColor = vec4(texColor.rgb * vLight, texColor.a);
}
)";

const char* CLOUD_VERT_SRC = R"(#version 300 es
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec4 vColor;

void main() {
    vColor = aColor;
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)";

const char* CLOUD_FRAG_SRC = R"(#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

// -------------------------------------------------------------
// شيدر الواجهة 2D (للأزرار وشاشات اللمس)
// -------------------------------------------------------------
const char* UI_VERT_SRC = R"(#version 300 es
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uOrtho;
out vec4 vColor;

void main() {
    vColor = aColor;
    gl_Position = uOrtho * vec4(aPos, 0.0, 1.0);
}
)";

const char* UI_FRAG_SRC = R"(#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

struct CloudVertex {
    glm::vec3 pos;
    glm::vec4 color;
};

struct UIVertex {
    glm::vec2 pos;
    glm::vec4 color;
};

struct Engine {
    struct android_app* app;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int32_t width = 0;
    int32_t height = 0;
    bool animating = false;

    GLuint blockShader = 0;
    GLuint cloudShader = 0;
    GLuint uiShader = 0;
    GLuint textureArrayID = 0;

    ChunkSection* chunk = nullptr;

    GLuint cloudVAO = 0, cloudVBO = 0;
    GLsizei cloudVertexCount = 0;

    GLuint uiVAO = 0, uiVBO = 0;

    Camera camera;
    float rotationAngle = 0.0f;
    float windOffset = 0.0f;

    // حالة دوران العالم (يمكن إيقافها وتشغيلها باللمس!)
    bool isRotating = true;

    // موقع وأبعاد زر الإيقاف والتشغيل على الشاشة
    float btnX = 0.0f, btnY = 0.0f, btnW = 100.0f, btnH = 100.0f;
};

static void buildCloudMesh(Engine* engine) {
    std::vector<CloudVertex> vertices;
    constexpr int GRID = 24;
    constexpr float CELL = 3.5f;
    constexpr float BASE_Y = 18.0f;
    constexpr float THICKNESS = 1.8f;

    auto hasCloud = [](int x, int z) {
        float n = smoothNoise(x * 0.2f + 5.0f, z * 0.2f + 5.0f);
        return n > 0.40f;
    };

    const glm::vec4 colTop    = glm::vec4(1.0f, 1.0f, 1.0f, 0.85f);
    const glm::vec4 colBottom = glm::vec4(0.72f, 0.75f, 0.82f, 0.80f);
    const glm::vec4 colSideX  = glm::vec4(0.88f, 0.90f, 0.95f, 0.82f);
    const glm::vec4 colSideZ  = glm::vec4(0.80f, 0.83f, 0.88f, 0.82f);

    float offset = (GRID * CELL) * 0.5f;

    for (int z = 0; z < GRID; ++z) {
        for (int x = 0; x < GRID; ++x) {
            if (!hasCloud(x, z)) continue;

            float x0 = (x * CELL) - offset, x1 = x0 + CELL;
            float z0 = (z * CELL) - offset, z1 = z0 + CELL;
            float y0 = BASE_Y, y1 = BASE_Y + THICKNESS;

            vertices.push_back({ {x0, y0, z0}, colBottom });
            vertices.push_back({ {x1, y0, z0}, colBottom });
            vertices.push_back({ {x1, y0, z1}, colBottom });
            vertices.push_back({ {x1, y0, z1}, colBottom });
            vertices.push_back({ {x0, y0, z1}, colBottom });
            vertices.push_back({ {x0, y0, z0}, colBottom });

            vertices.push_back({ {x0, y1, z1}, colTop });
            vertices.push_back({ {x1, y1, z1}, colTop });
            vertices.push_back({ {x1, y1, z0}, colTop });
            vertices.push_back({ {x1, y1, z0}, colTop });
            vertices.push_back({ {x0, y1, z0}, colTop });
            vertices.push_back({ {x0, y1, z1}, colTop });

            if (z == 0 || !hasCloud(x, z - 1)) {
                vertices.push_back({ {x1, y0, z0}, colSideZ });
                vertices.push_back({ {x0, y0, z0}, colSideZ });
                vertices.push_back({ {x0, y1, z0}, colSideZ });
                vertices.push_back({ {x0, y1, z0}, colSideZ });
                vertices.push_back({ {x1, y1, z0}, colSideZ });
                vertices.push_back({ {x1, y0, z0}, colSideZ });
            }
            if (z == GRID - 1 || !hasCloud(x, z + 1)) {
                vertices.push_back({ {x0, y0, z1}, colSideZ });
                vertices.push_back({ {x1, y0, z1}, colSideZ });
                vertices.push_back({ {x1, y1, z1}, colSideZ });
                vertices.push_back({ {x1, y1, z1}, colSideZ });
                vertices.push_back({ {x0, y1, z1}, colSideZ });
                vertices.push_back({ {x0, y0, z1}, colSideZ });
            }
            if (x == 0 || !hasCloud(x - 1, z)) {
                vertices.push_back({ {x0, y0, z0}, colSideX });
                vertices.push_back({ {x0, y0, z1}, colSideX });
                vertices.push_back({ {x0, y1, z1}, colSideX });
                vertices.push_back({ {x0, y1, z1}, colSideX });
                vertices.push_back({ {x0, y1, z0}, colSideX });
                vertices.push_back({ {x0, y0, z0}, colSideX });
            }
            if (x == GRID - 1 || !hasCloud(x + 1, z)) {
                vertices.push_back({ {x1, y0, z1}, colSideX });
                vertices.push_back({ {x1, y0, z0}, colSideX });
                vertices.push_back({ {x1, y1, z0}, colSideX });
                vertices.push_back({ {x1, y1, z0}, colSideX });
                vertices.push_back({ {x1, y1, z1}, colSideX });
                vertices.push_back({ {x1, y0, z1}, colSideX });
            }
        }
    }

    engine->cloudVertexCount = static_cast<GLsizei>(vertices.size());

    glGenVertexArrays(1, &engine->cloudVAO);
    glGenBuffers(1, &engine->cloudVBO);

    glBindVertexArray(engine->cloudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, engine->cloudVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(CloudVertex), vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CloudVertex), (void*)offsetof(CloudVertex, pos));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CloudVertex), (void*)offsetof(CloudVertex, color));

    glBindVertexArray(0);
}

static GLuint loadTexturesFromApk(AAssetManager* mgr) {
    std::vector<std::string> textureNames = {
        "grass_block_top.png", "grass_block_side.png", "dirt.png", "stone.png", "oak_leaves.png"
    };

    GLuint texID = 0;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texID);

    int layerCount = static_cast<int>(textureNames.size());
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 16, 16, layerCount, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    stbi_set_flip_vertically_on_load(1);

    for (int i = 0; i < layerCount; ++i) {
        std::string filename = textureNames[i];
        AAsset* asset = AAssetManager_open(mgr, ("textures/blocks/" + filename).c_str(), AASSET_MODE_BUFFER);
        if (!asset) asset = AAssetManager_open(mgr, ("res/textures/blocks/" + filename).c_str(), AASSET_MODE_BUFFER);

        if (asset) {
            size_t size = AAsset_getLength(asset);
            const void* buffer = AAsset_getBuffer(asset);
            int w, h, channels;
            unsigned char* data = stbi_load_from_memory((const unsigned char*)buffer, (int)size, &w, &h, &channels, 4);
            if (data) {
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
                stbi_image_free(data);
            }
            AAsset_close(asset);
        } else {
            std::vector<uint32_t> fallback(16 * 16, (i == 3 ? 0xFF888888 : 0xFF22AA22));
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, 16, 16, 1, GL_RGBA, GL_UNSIGNED_BYTE, fallback.data());
        }
    }

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    return texID;
}

static GLuint compileProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, nullptr);
    glCompileShader(fs);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// دالة رسم مستطيل ملون في الواجهة 2D
static void addQuad(std::vector<UIVertex>& v, float x0, float y0, float x1, float y1, glm::vec4 col) {
    v.push_back({ {x0, y0}, col });
    v.push_back({ {x1, y0}, col });
    v.push_back({ {x1, y1}, col });
    v.push_back({ {x1, y1}, col });
    v.push_back({ {x0, y1}, col });
    v.push_back({ {x0, y0}, col });
}

// رسم زر الإيقاف / التشغيل (Pause / Play)
static void renderUI(Engine* engine) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(engine->uiShader);

    // مصفوفة إسقاط 2D مطابقة لبكسلات شاشة الهاتف
    glm::mat4 ortho = glm::ortho(0.0f, (float)engine->width, (float)engine->height, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(engine->uiShader, "uOrtho"), 1, GL_FALSE, &ortho[0][0]);

    std::vector<UIVertex> uiVertices;

    float bx = engine->btnX;
    float by = engine->btnY;
    float bw = engine->btnW;
    float bh = engine->btnH;

    // 1. خلفية الزر (مربع رمادي شفاف بستايل ماينكرافت)
    glm::vec4 bgCol = glm::vec4(0.12f, 0.12f, 0.14f, 0.75f);
    addQuad(uiVertices, bx, by, bx + bw, by + bh, bgCol);

    // إطار الزر العلوي والأيسر (أفتح لوناً)
    glm::vec4 borderLight = glm::vec4(0.5f, 0.5f, 0.55f, 0.85f);
    addQuad(uiVertices, bx, by, bx + bw, by + 4.0f, borderLight);
    addQuad(uiVertices, bx, by, bx + 4.0f, by + bh, borderLight);

    // إطار الزر السفلي والأيمن (أغمق لوناً)
    glm::vec4 borderDark = glm::vec4(0.05f, 0.05f, 0.08f, 0.9f);
    addQuad(uiVertices, bx, by + bh - 4.0f, bx + bw, by + bh, borderDark);
    addQuad(uiVertices, bx + bw - 4.0f, by, bx + bw, by + bh, borderDark);

    // 2. رسم الأيقونة داخل الزر:
    if (engine->isRotating) {
        // إذا كان العالم يدور: نرسم أيقونة إيقاف مؤقت Pause || (خطين رأسيين أبيضين)
        glm::vec4 barCol = glm::vec4(1.0f, 1.0f, 1.0f, 0.95f);
        addQuad(uiVertices, bx + 30.0f, by + 25.0f, bx + 42.0f, by + bh - 25.0f, barCol);
        addQuad(uiVertices, bx + 58.0f, by + 25.0f, bx + 70.0f, by + bh - 25.0f, barCol);
    } else {
        // إذا كان العالم متوقفاً: نرسم أيقونة تشغيل Play ▶ (مثلث أخضر زاهي)
        glm::vec4 playCol = glm::vec4(0.3f, 0.9f, 0.3f, 1.0f);
        uiVertices.push_back({ {bx + 35.0f, by + 25.0f}, playCol });
        uiVertices.push_back({ {bx + 75.0f, by + bh * 0.5f}, playCol });
        uiVertices.push_back({ {bx + 35.0f, by + bh - 25.0f}, playCol });
    }

    glBindVertexArray(engine->uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, engine->uiVBO);
    glBufferData(GL_ARRAY_BUFFER, uiVertices.size() * sizeof(UIVertex), uiVertices.data(), GL_STREAM_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)uiVertices.size());
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

// -------------------------------------------------------------
// نظام معالجة اللمس (Touch Input System)
// -------------------------------------------------------------
static int32_t handleInput(struct android_app* app, AInputEvent* event) {
    auto* engine = (Engine*)app->userData;

    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event);
        int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;

        // عند لمس الشاشة بإصبع
        if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            size_t pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
            float touchX = AMotionEvent_getX(event, pointerIndex);
            float touchY = AMotionEvent_getY(event, pointerIndex);

            // فحص التصادم: هل اللمس يقع داخل حدود زر الإيقاف؟
            if (touchX >= engine->btnX && touchX <= engine->btnX + engine->btnW &&
                touchY >= engine->btnY && touchY <= engine->btnY + engine->btnH) {
                
                // قلب حالة الدوران فوراً!
                engine->isRotating = !engine->isRotating;
                LOGI("Rotation Toggled! isRotating = %d", engine->isRotating);
                return 1; // تم التعامل مع اللمس بنجاح
            }
        }
    }
    return 0;
}

static int initDisplay(Engine* engine) {
    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 24, EGL_NONE
    };

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    EGLSurface surface = eglCreateWindowSurface(display, config, engine->app->window, nullptr);
    const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) return -1;

    eglQuerySurface(display, surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &engine->height);

    engine->display = display;
    engine->surface = surface;
    engine->context = context;

    // وضع الزر في أعلى يمين الشاشة مع هامش مريح للإبهام
    engine->btnW = 100.0f;
    engine->btnH = 100.0f;
    engine->btnX = (float)engine->width - engine->btnW - 50.0f;
    engine->btnY = 40.0f;

    glViewport(0, 0, engine->width, engine->height);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    engine->blockShader = compileProgram(VERTEX_SHADER_SRC, FRAGMENT_SHADER_SRC);
    engine->cloudShader = compileProgram(CLOUD_VERT_SRC, CLOUD_FRAG_SRC);
    engine->uiShader    = compileProgram(UI_VERT_SRC, UI_FRAG_SRC);

    // تجهيز VAO/VBO الخاص بالواجهة 2D
    glGenVertexArrays(1, &engine->uiVAO);
    glGenBuffers(1, &engine->uiVBO);
    glBindVertexArray(engine->uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, engine->uiVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)offsetof(UIVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)offsetof(UIVertex, color));
    glBindVertexArray(0);

    engine->textureArrayID = loadTexturesFromApk(engine->app->activity->assetManager);

    // بناء الـ Chunk
    engine->chunk = new ChunkSection(0, 0, 0);
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            int h = getTerrainHeight(x, z);
            for (int y = 0; y <= h; ++y) {
                if (y == h) engine->chunk->setBlock(x, y, z, BlockType::Grass);
                else if (y >= h - 2) engine->chunk->setBlock(x, y, z, BlockType::Dirt);
                else engine->chunk->setBlock(x, y, z, BlockType::Stone);
            }
        }
    }

    // زراعة الشجرة الكلاسيكية
    int tx = 8, tz = 8;
    int ty = getTerrainHeight(tx, tz) + 1;
    engine->chunk->setBlock(tx, ty, tz, BlockType::Dirt);
    engine->chunk->setBlock(tx, ty + 1, tz, BlockType::Dirt);
    engine->chunk->setBlock(tx, ty + 2, tz, BlockType::Dirt);
    for (int ox = -1; ox <= 1; ++ox) {
        for (int oz = -1; oz <= 1; ++oz) {
            engine->chunk->setBlock(tx + ox, ty + 2, tz + oz, BlockType::OakLeaves);
            engine->chunk->setBlock(tx + ox, ty + 3, tz + oz, BlockType::OakLeaves);
        }
    }
    engine->chunk->setBlock(tx, ty + 4, tz, BlockType::OakLeaves);
    engine->chunk->setBlock(tx + 1, ty + 4, tz, BlockType::OakLeaves);
    engine->chunk->setBlock(tx - 1, ty + 4, tz, BlockType::OakLeaves);
    engine->chunk->setBlock(tx, ty + 4, tz + 1, BlockType::OakLeaves);
    engine->chunk->setBlock(tx, ty + 4, tz - 1, BlockType::OakLeaves);

    engine->chunk->buildMesh();
    buildCloudMesh(engine);

    engine->camera.position = glm::vec3(8.0f, 17.0f, 28.0f);
    engine->camera.front = glm::normalize(glm::vec3(8.0f, 5.0f, 8.0f) - engine->camera.position);

    return 0;
}

static void drawFrame(Engine* engine) {
    if (engine->display == EGL_NO_DISPLAY) return;

    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)engine->width / (float)engine->height;
    glm::mat4 projection = engine->camera.getProjectionMatrix(aspect);
    glm::mat4 view = engine->camera.getViewMatrix();

    // 1. رسم مجسم العالم 3D
    glUseProgram(engine->blockShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, engine->textureArrayID);
    glUniform1i(glGetUniformLocation(engine->blockShader, "uTextureArray"), 0);

    glm::mat4 modelChunk = glm::mat4(1.0f);

    // إذا كانت حالة الدوران مفعلة نزيد الزاوية، وإذا ضغطت زر الإيقاف تتجمد!
    if (engine->isRotating) {
        engine->rotationAngle += 0.009f;
    }

    modelChunk = glm::translate(modelChunk, glm::vec3(8.0f, 0.0f, 8.0f));
    modelChunk = glm::rotate(modelChunk, engine->rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    modelChunk = glm::translate(modelChunk, glm::vec3(-8.0f, 0.0f, -8.0f));

    glUniformMatrix4fv(glGetUniformLocation(engine->blockShader, "uProjection"), 1, GL_FALSE, &projection[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(engine->blockShader, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(engine->blockShader, "uModel"), 1, GL_FALSE, &modelChunk[0][0]);

    engine->chunk->render();

    // 2. رسم الغيوم ثلاثية الأبعاد 3D
    glUseProgram(engine->cloudShader);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (engine->isRotating) {
        engine->windOffset += 0.025f;
        if (engine->windOffset > 3.5f * 24.0f) engine->windOffset = 0.0f;
    }

    glm::mat4 modelCloud = glm::mat4(1.0f);
    modelCloud = glm::translate(modelCloud, glm::vec3(8.0f + engine->windOffset, 0.0f, 8.0f));
    modelCloud = glm::rotate(modelCloud, engine->rotationAngle * 0.3f, glm::vec3(0.0f, 1.0f, 0.0f));
    modelCloud = glm::translate(modelCloud, glm::vec3(-8.0f, 0.0f, -8.0f));

    glUniformMatrix4fv(glGetUniformLocation(engine->cloudShader, "uProjection"), 1, GL_FALSE, &projection[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(engine->cloudShader, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(engine->cloudShader, "uModel"), 1, GL_FALSE, &modelCloud[0][0]);

    glBindVertexArray(engine->cloudVAO);
    glDrawArrays(GL_TRIANGLES, 0, engine->cloudVertexCount);
    glBindVertexArray(0);

    glDisable(GL_BLEND);

    // 3. رسم واجهة اللمس 2D (زر الإيقاف والتشغيل التفاعلي)
    renderUI(engine);

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
    
    // ربط مستشعر اللمس لشاشة الهاتف!
    state->onInputEvent = handleInput;

    engine.app = state;

    LOGI("Minecraft Android Engine with Touch UI started!");

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
