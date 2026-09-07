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

#include "ChunkSection.hpp"
#include "Camera.hpp"
#include "stb_image.h"

#define LOG_TAG "MinecraftClone"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// -------------------------------------------------------------
// الشيدرات مدمجة (Blocks, Clouds, UI)
// -------------------------------------------------------------
static const char* BLOCK_VERT = R"(#version 300 es
layout(location=0) in uint a; uniform mat4 uModel, uView, uProj;
out vec2 vUV; flat out int vLayer; out float vLight;
const vec2 UV_C[4] = vec2[4](vec2(0,0), vec2(1,0), vec2(1,1), vec2(0,1));
const float LIGHTS[6] = float[6](1.0, 0.5, 0.8, 0.8, 0.6, 0.6);
void main() {
    uint x = a & 31u, y = (a >> 5u) & 31u, z = (a >> 10u) & 31u;
    uint dir = (a >> 15u) & 7u, layer = (a >> 20u) & 1023u, uv = (a >> 30u) & 3u;
    vUV = UV_C[uv]; vLayer = int(layer); vLight = LIGHTS[dir];
    gl_Position = uProj * uView * uModel * vec4(float(x), float(y), float(z), 1.0);
})";

static const char* BLOCK_FRAG = R"(#version 300 es
precision mediump float; precision mediump sampler2DArray;
in vec2 vUV; flat in int vLayer; in float vLight;
uniform sampler2DArray uTex; out vec4 FragColor;
void main() {
    vec4 col = texture(uTex, vec3(vUV, float(vLayer)));
    if (col.a < 0.5) discard;
    FragColor = vec4(col.rgb * vLight, col.a);
})";

static const char* CLOUD_VERT = R"(#version 300 es
layout(location=0) in vec3 aPos; layout(location=1) in vec4 aCol;
uniform mat4 uModel, uView, uProj; out vec4 vCol;
void main() { vCol = aCol; gl_Position = uProj * uView * uModel * vec4(aPos, 1.0); })";

static const char* CLOUD_FRAG = R"(#version 300 es
precision mediump float; in vec4 vCol; out vec4 FragColor;
void main() { FragColor = vCol; })";

static const char* UI_VERT = R"(#version 300 es
layout(location=0) in vec2 aPos; layout(location=1) in vec4 aCol;
uniform mat4 uOrtho; out vec4 vCol;
void main() { vCol = aCol; gl_Position = uOrtho * vec4(aPos, 0.0, 1.0); })";

static const char* UI_FRAG = R"(#version 300 es
precision mediump float; in vec4 vCol; out vec4 FragColor;
void main() { FragColor = vCol; })";

static GLuint makeProgram(const char* vs, const char* fs) {
    GLuint v = glCreateShader(GL_VERTEX_SHADER); glShaderSource(v, 1, &vs, nullptr); glCompileShader(v);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(f, 1, &fs, nullptr); glCompileShader(f);
    GLuint p = glCreateProgram(); glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f); return p;
}

// -------------------------------------------------------------
// رياضيات التضاريس الطبيعية
// -------------------------------------------------------------
static float hash2D(int x, int z) {
    int n = x + z * 57; n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

static float smoothNoise(float x, float z) {
    int ix = (int)std::floor(x), iz = (int)std::floor(z);
    float fx = x - ix, fz = z - iz;
    fx = fx * fx * (3.0f - 2.0f * fx); fz = fz * fz * (3.0f - 2.0f * fz);
    float s00 = hash2D(ix, iz), s10 = hash2D(ix + 1, iz);
    float s01 = hash2D(ix, iz + 1), s11 = hash2D(ix + 1, iz + 1);
    return (s00 * (1.0f - fx) + s10 * fx) * (1.0f - fz) + (s01 * (1.0f - fx) + s11 * fx) * fz;
}

static int getTerrainHeight(int x, int z) {
    float n = smoothNoise(x * 0.12f, z * 0.12f) * 3.5f + smoothNoise(x * 0.25f, z * 0.25f) * 1.5f;
    int h = 4 + (int)n; return (h < 2) ? 2 : ((h > 9) ? 9 : h);
}

struct CloudVertex { glm::vec3 pos; glm::vec4 color; };
struct UIVertex { glm::vec2 pos; glm::vec4 color; };

struct Engine {
    struct android_app* app;
    EGLDisplay display = EGL_NO_DISPLAY; EGLSurface surface = EGL_NO_SURFACE; EGLContext context = EGL_NO_CONTEXT;
    int32_t width = 0, height = 0; bool animating = false;
    GLuint blockShader = 0, cloudShader = 0, uiShader = 0, texArray = 0;
    ChunkSection* chunk = nullptr;
    GLuint cloudVAO = 0, cloudVBO = 0; GLsizei cloudCount = 0;
    GLuint uiVAO = 0, uiVBO = 0;
    Camera camera;
    float rotAngle = 0.0f, windOffset = 0.0f;
    bool isRotating = true;
    float btnX = 0, btnY = 0, btnW = 100, btnH = 100;
};

static void initClouds(Engine* e) {
    std::vector<CloudVertex> v;
    constexpr int G = 24; constexpr float C = 3.5f, Y0 = 18.0f, Y1 = 19.8f, off = (G * C) * 0.5f;
    auto has = [](int x, int z) { return smoothNoise(x * 0.2f + 5.0f, z * 0.2f + 5.0f) > 0.40f; };
    glm::vec4 cT(1,1,1,0.85f), cB(0.72f,0.75f,0.82f,0.8f), cS(0.85f,0.88f,0.92f,0.82f);
    for (int z = 0; z < G; ++z) for (int x = 0; x < G; ++x) {
        if (!has(x, z)) continue;
        float x0 = x * C - off, x1 = x0 + C, z0 = z * C - off, z1 = z0 + C;
        v.push_back({{x0,Y0,z0},cB}); v.push_back({{x1,Y0,z0},cB}); v.push_back({{x1,Y0,z1},cB});
        v.push_back({{x1,Y0,z1},cB}); v.push_back({{x0,Y0,z1},cB}); v.push_back({{x0,Y0,z0},cB});
        v.push_back({{x0,Y1,z1},cT}); v.push_back({{x1,Y1,z1},cT}); v.push_back({{x1,Y1,z0},cT});
        v.push_back({{x1,Y1,z0},cT}); v.push_back({{x0,Y1,z0},cT}); v.push_back({{x0,Y1,z1},cT});
        if (z==0 || !has(x, z-1)) { v.push_back({{x1,Y0,z0},cS}); v.push_back({{x0,Y0,z0},cS}); v.push_back({{x0,Y1,z0},cS}); v.push_back({{x0,Y1,z0},cS}); v.push_back({{x1,Y1,z0},cS}); v.push_back({{x1,Y0,z0},cS}); }
        if (z==G-1 || !has(x, z+1)) { v.push_back({{x0,Y0,z1},cS}); v.push_back({{x1,Y0,z1},cS}); v.push_back({{x1,Y1,z1},cS}); v.push_back({{x1,Y1,z1},cS}); v.push_back({{x0,Y1,z1},cS}); v.push_back({{x0,Y0,z1},cS}); }
        if (x==0 || !has(x-1, z)) { v.push_back({{x0,Y0,z0},cS}); v.push_back({{x0,Y0,z1},cS}); v.push_back({{x0,Y1,z1},cS}); v.push_back({{x0,Y1,z1},cS}); v.push_back({{x0,Y1,z0},cS}); v.push_back({{x0,Y0,z0},cS}); }
        if (x==G-1 || !has(x+1, z)) { v.push_back({{x1,Y0,z1},cS}); v.push_back({{x1,Y0,z0},cS}); v.push_back({{x1,Y1,z0},cS}); v.push_back({{x1,Y1,z0},cS}); v.push_back({{x1,Y1,z1},cS}); v.push_back({{x1,Y0,z1},cS}); }
    }
    e->cloudCount = (GLsizei)v.size();
    glGenVertexArrays(1, &e->cloudVAO); glGenBuffers(1, &e->cloudVBO);
    glBindVertexArray(e->cloudVAO); glBindBuffer(GL_ARRAY_BUFFER, e->cloudVBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(CloudVertex), v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CloudVertex), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CloudVertex), (void*)12);
    glBindVertexArray(0);
}

static GLuint loadTextures(AAssetManager* mgr) {
    std::vector<std::string> names = {"grass_block_top.png", "grass_block_side.png", "dirt.png", "stone.png", "oak_leaves.png"};
    GLuint id; glGenTextures(1, &id); glBindTexture(GL_TEXTURE_2D_ARRAY, id);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 16, 16, (GLsizei)names.size(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    stbi_set_flip_vertically_on_load(1);
    for (int i = 0; i < (int)names.size(); ++i) {
        AAsset* a = AAssetManager_open(mgr, ("textures/blocks/" + names[i]).c_str(), AASSET_MODE_BUFFER);
        if (!a) a = AAssetManager_open(mgr, ("res/textures/blocks/" + names[i]).c_str(), AASSET_MODE_BUFFER);
        if (a) {
            int w, h, c;
            unsigned char* data = stbi_load_from_memory((const unsigned char*)AAsset_getBuffer(a), (int)AAsset_getLength(a), &w, &h, &c, 4);
            if (data) { glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, data); stbi_image_free(data); }
            AAsset_close(a);
        }
    }
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY); return id;
}

static void renderTouchUI(Engine* e) {
    glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(e->uiShader);
    glm::mat4 ortho = glm::ortho(0.0f, (float)e->width, (float)e->height, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(e->uiShader, "uOrtho"), 1, GL_FALSE, &ortho[0][0]);

    std::vector<UIVertex> v;
    auto addQ = [&](float x0, float y0, float x1, float y1, glm::vec4 c) {
        v.push_back({{x0,y0},c}); v.push_back({{x1,y0},c}); v.push_back({{x1,y1},c});
        v.push_back({{x1,y1},c}); v.push_back({{x0,y1},c}); v.push_back({{x0,y0},c});
    };

    float bx = e->btnX, by = e->btnY, bw = e->btnW, bh = e->btnH;
    addQ(bx, by, bx + bw, by + bh, {0.12f, 0.12f, 0.14f, 0.75f});
    addQ(bx, by, bx + bw, by + 4, {0.5f, 0.5f, 0.55f, 0.85f});
    addQ(bx, by, bx + 4, by + bh, {0.5f, 0.5f, 0.55f, 0.85f});
    addQ(bx, by + bh - 4, bx + bw, by + bh, {0.05f, 0.05f, 0.08f, 0.9f});
    addQ(bx + bw - 4, by, bx + bw, by + bh, {0.05f, 0.05f, 0.08f, 0.9f});

    if (e->isRotating) {
        addQ(bx + 30, by + 25, bx + 42, by + bh - 25, {1,1,1,0.95f});
        addQ(bx + 58, by + 25, bx + 70, by + bh - 25, {1,1,1,0.95f});
    } else {
        v.push_back({{bx + 35, by + 25}, {0.3f, 0.9f, 0.3f, 1}});
        v.push_back({{bx + 75, by + bh * 0.5f}, {0.3f, 0.9f, 0.3f, 1}});
        v.push_back({{bx + 35, by + bh - 25}, {0.3f, 0.9f, 0.3f, 1}});
    }

    glBindVertexArray(e->uiVAO); glBindBuffer(GL_ARRAY_BUFFER, e->uiVBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(UIVertex), v.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)v.size());
    glBindVertexArray(0); glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
}

static int initDisplay(Engine* e) {
    const EGLint attribs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE };
    e->display = eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(e->display, nullptr, nullptr);
    EGLConfig cfg; EGLint numCfg; eglChooseConfig(e->display, attribs, &cfg, 1, &numCfg);
    e->surface = eglCreateWindowSurface(e->display, cfg, e->app->window, nullptr);
    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    e->context = eglCreateContext(e->display, cfg, EGL_NO_CONTEXT, ctxAttribs);
    eglMakeCurrent(e->display, e->surface, e->surface, e->context);
    eglQuerySurface(e->display, e->surface, EGL_WIDTH, &e->width);
    eglQuerySurface(e->display, e->surface, EGL_HEIGHT, &e->height);

    e->btnW = e->btnH = 100.0f;
    e->btnX = (float)e->width - e->btnW - 50.0f; e->btnY = 40.0f;

    glViewport(0, 0, e->width, e->height);
    glEnable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);

    e->blockShader = makeProgram(BLOCK_VERT, BLOCK_FRAG);
    e->cloudShader = makeProgram(CLOUD_VERT, CLOUD_FRAG);
    e->uiShader    = makeProgram(UI_VERT, UI_FRAG);

    glGenVertexArrays(1, &e->uiVAO); glGenBuffers(1, &e->uiVBO);
    glBindVertexArray(e->uiVAO); glBindBuffer(GL_ARRAY_BUFFER, e->uiVBO);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)8);
    glBindVertexArray(0);

    e->texArray = loadTextures(e->app->activity->assetManager);
    initClouds(e);

    e->chunk = new ChunkSection(0, 0, 0);
    for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) {
        int h = getTerrainHeight(x, z);
        for (int y = 0; y <= h; ++y) {
            if (y == h) e->chunk->setBlock(x, y, z, BlockType::Grass);
            else if (y >= h - 2) e->chunk->setBlock(x, y, z, BlockType::Dirt);
            else e->chunk->setBlock(x, y, z, BlockType::Stone);
        }
    }
    int ty = getTerrainHeight(8, 8) + 1;
    for (int y = 0; y < 3; ++y) e->chunk->setBlock(8, ty + y, 8, BlockType::Dirt);
    for (int ox = -1; ox <= 1; ++ox) for (int oz = -1; oz <= 1; ++oz) {
        e->chunk->setBlock(8 + ox, ty + 2, 8 + oz, BlockType::OakLeaves);
        e->chunk->setBlock(8 + ox, ty + 3, 8 + oz, BlockType::OakLeaves);
    }
    e->chunk->setBlock(8, ty + 4, 8, BlockType::OakLeaves);
    e->chunk->buildMesh();

    e->camera.position = glm::vec3(8.0f, 17.0f, 28.0f);
    e->camera.front = glm::normalize(glm::vec3(8.0f, 5.0f, 8.0f) - e->camera.position);
    return 0;
}

static void drawFrame(Engine* e) {
    if (e->display == EGL_NO_DISPLAY) return;
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)e->width / (float)e->height;
    glm::mat4 proj = e->camera.getProjectionMatrix(aspect), view = e->camera.getViewMatrix();

    if (e->isRotating) { e->rotAngle += 0.009f; e->windOffset += 0.025f; }

    glUseProgram(e->blockShader);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D_ARRAY, e->texArray);
    glm::mat4 mChunk = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(8,0,8)), e->rotAngle, glm::vec3(0,1,0));
    mChunk = glm::translate(mChunk, glm::vec3(-8,0,-8));
    glUniformMatrix4fv(glGetUniformLocation(e->blockShader, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(e->blockShader, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(e->blockShader, "uModel"), 1, GL_FALSE, &mChunk[0][0]);
    e->chunk->render();

    glUseProgram(e->cloudShader); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glm::mat4 mCloud = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(8 + e->windOffset, 0, 8)), e->rotAngle * 0.3f, glm::vec3(0,1,0));
    mCloud = glm::translate(mCloud, glm::vec3(-8,0,-8));
    glUniformMatrix4fv(glGetUniformLocation(e->cloudShader, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(e->cloudShader, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(e->cloudShader, "uModel"), 1, GL_FALSE, &mCloud[0][0]);
    glBindVertexArray(e->cloudVAO); glDrawArrays(GL_TRIANGLES, 0, e->cloudCount); glBindVertexArray(0);
    glDisable(GL_BLEND);

    renderTouchUI(e);
    eglSwapBuffers(e->display, e->surface);
}

static int32_t handleInput(struct android_app* app, AInputEvent* ev) {
    auto* e = (Engine*)app->userData;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int act = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
        if (act == AMOTION_EVENT_ACTION_DOWN || act == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            float x = AMotionEvent_getX(ev, 0), y = AMotionEvent_getY(ev, 0);
            if (x >= e->btnX && x <= e->btnX + e->btnW && y >= e->btnY && y <= e->btnY + e->btnH) {
                e->isRotating = !e->isRotating;
                return 1;
            }
        }
    }
    return 0;
}

static void handleCmd(struct android_app* app, int32_t cmd) {
    auto* e = (Engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW && e->app->window) { initDisplay(e); e->animating = true; }
    else if (cmd == APP_CMD_TERM_WINDOW) { e->animating = false; }
    else if (cmd == APP_CMD_GAINED_FOCUS) { e->animating = true; }
    else if (cmd == APP_CMD_LOST_FOCUS) { e->animating = false; }
}

extern "C" void android_main(struct android_app* state) {
    Engine e{}; state->userData = &e; state->onAppCmd = handleCmd; state->onInputEvent = handleInput; e.app = state;
    while (true) {
        int ev; struct android_poll_source* src;
        while (ALooper_pollOnce(e.animating ? 0 : -1, nullptr, &ev, (void**)&src) >= 0) {
            if (src) src->process(state, src);
            if (state->destroyRequested) return;
        }
        if (e.animating) drawFrame(&e);
    }
}

#endif
