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
#include <chrono>

#include "ChunkSection.hpp"
#include "Camera.hpp"
#include "stb_image.h"

#define LOG_TAG "MinecraftClone"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// -------------------------------------------------------------
// شيدرات 3D للعالم والغيوم
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

// -------------------------------------------------------------
// شيدر الواجهة 2D (للأزرار وعلامة التصويب)
// -------------------------------------------------------------
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

// -------------------------------------------------------------
// كائن التحكم باللاعب (First Person Player Controller)
// -------------------------------------------------------------
struct Player {
    glm::vec3 position{8.0f, 10.0f, 8.0f};
    glm::vec3 velocity{0.0f};
    float yaw = -90.0f;
    float pitch = 0.0f;
    bool isGrounded = false;
    const float eyeHeight = 1.62f;
    const float speed = 5.0f;
    const float gravity = -18.0f;
    const float jumpForce = 7.5f;
};

// -------------------------------------------------------------
// نظام اللمس المتعدد (Multi-Touch State)
// -------------------------------------------------------------
struct TouchInput {
    int movePointerId = -1;
    glm::vec2 moveInput{0.0f};

    int lookPointerId = -1;
    glm::vec2 lastLookPos{0.0f};

    bool jumpPressed = false;
    int jumpPointerId = -1;
};

struct Engine {
    struct android_app* app;
    EGLDisplay display = EGL_NO_DISPLAY; EGLSurface surface = EGL_NO_SURFACE; EGLContext context = EGL_NO_CONTEXT;
    int32_t width = 0, height = 0; bool animating = false;

    GLuint blockShader = 0, cloudShader = 0, uiShader = 0, texArray = 0;
    ChunkSection* chunk = nullptr;

    GLuint cloudVAO = 0, cloudVBO = 0; GLsizei cloudCount = 0;
    GLuint uiVAO = 0, uiVBO = 0;

    Player player;
    TouchInput touch;
    Camera camera; // تمت إضافة تعريف الكاميرا هنا بنجاح!

    float windOffset = 0.0f;
    std::chrono::steady_clock::time_point lastFrameTime;
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

// -------------------------------------------------------------
// رسم الواجهة المجمعة دفعة واحدة (Single Draw Call UI)
// -------------------------------------------------------------
static void renderAllUI(Engine* e) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(e->uiShader);
    glm::mat4 ortho = glm::ortho(0.0f, (float)e->width, (float)e->height, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(e->uiShader, "uOrtho"), 1, GL_FALSE, &ortho[0][0]);

    std::vector<UIVertex> v;
    auto addQuad = [&](float x0, float y0, float x1, float y1, glm::vec4 col) {
        v.push_back({{x0,y0}, col}); v.push_back({{x1,y0}, col}); v.push_back({{x1,y1}, col});
        v.push_back({{x1,y1}, col}); v.push_back({{x0,y1}, col}); v.push_back({{x0,y0}, col});
    };

    // 1. مؤشر التصويب في المنتصف (Crosshair +)
    float cx = (float)e->width * 0.5f;
    float cy = (float)e->height * 0.5f;
    glm::vec4 crossCol(1.0f, 1.0f, 1.0f, 0.75f);
    addQuad(cx - 10.0f, cy - 2.0f, cx + 10.0f, cy + 2.0f, crossCol);
    addQuad(cx - 2.0f, cy - 10.0f, cx + 2.0f, cy + 10.0f, crossCol);

    // 2. أزرار الـ D-Pad (يسار الشاشة)
    float dpadX = 180.0f;
    float dpadY = (float)e->height - 180.0f;
    float btnSize = 65.0f;
    glm::vec4 padBg(0.15f, 0.15f, 0.15f, 0.55f);
    glm::vec4 padActive(0.35f, 0.35f, 0.35f, 0.85f);

    // زر للأمام
    addQuad(dpadX - btnSize*0.5f, dpadY - btnSize*1.5f, dpadX + btnSize*0.5f, dpadY - btnSize*0.5f, 
            e->touch.moveInput.y > 0.1f ? padActive : padBg);
    // زر للخلف
    addQuad(dpadX - btnSize*0.5f, dpadY + btnSize*0.5f, dpadX + btnSize*0.5f, dpadY + btnSize*1.5f, 
            e->touch.moveInput.y < -0.1f ? padActive : padBg);
    // زر لليسار
    addQuad(dpadX - btnSize*1.5f, dpadY - btnSize*0.5f, dpadX - btnSize*0.5f, dpadY + btnSize*0.5f, 
            e->touch.moveInput.x < -0.1f ? padActive : padBg);
    // زر لليمين
    addQuad(dpadX + btnSize*0.5f, dpadY - btnSize*0.5f, dpadX + btnSize*1.5f, dpadY + btnSize*0.5f, 
            e->touch.moveInput.x > 0.1f ? padActive : padBg);
    // المركز
    addQuad(dpadX - btnSize*0.5f, dpadY - btnSize*0.5f, dpadX + btnSize*0.5f, dpadY + btnSize*0.5f, padBg);

    // 3. زر القفز (يمين الشاشة)
    float jX = (float)e->width - 160.0f;
    float jY = (float)e->height - 160.0f;
    float jSize = 85.0f;
    addQuad(jX - jSize*0.5f, jY - jSize*0.5f, jX + jSize*0.5f, jY + jSize*0.5f, 
            e->touch.jumpPressed ? padActive : padBg);

    // سهم القفز للأعلى
    glm::vec4 arrowCol(1.0f, 1.0f, 1.0f, 0.9f);
    v.push_back({{jX, jY - 20.0f}, arrowCol});
    v.push_back({{jX + 18.0f, jY + 12.0f}, arrowCol});
    v.push_back({{jX - 18.0f, jY + 12.0f}, arrowCol});

    glBindVertexArray(e->uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, e->uiVBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(UIVertex), v.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)v.size());
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

// -------------------------------------------------------------
// نظام معالجة اللمس المتعدد (Multi-Touch)
// -------------------------------------------------------------
static int32_t handleInput(struct android_app* app, AInputEvent* ev) {
    auto* e = (Engine*)app->userData;

    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(ev);
        int actionMasked = action & AMOTION_EVENT_ACTION_MASK;
        int pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int pointerId = AMotionEvent_getPointerId(ev, pointerIndex);

        float dpadX = 180.0f, dpadY = (float)e->height - 180.0f;
        float jX = (float)e->width - 160.0f, jY = (float)e->height - 160.0f;
        float jRadius = 60.0f;

        if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            float x = AMotionEvent_getX(ev, pointerIndex);
            float y = AMotionEvent_getY(ev, pointerIndex);

            // زر القفز
            if (glm::distance(glm::vec2(x, y), glm::vec2(jX, jY)) < jRadius) {
                e->touch.jumpPressed = true;
                e->touch.jumpPointerId = pointerId;
                if (e->player.isGrounded) {
                    e->player.velocity.y = e->player.jumpForce;
                    e->player.isGrounded = false;
                }
                return 1;
            }

            // منطقة الحركة D-Pad (اليسار)
            if (x < (float)e->width * 0.45f) {
                e->touch.movePointerId = pointerId;
                glm::vec2 diff = glm::vec2(x, y) - glm::vec2(dpadX, dpadY);
                if (glm::length(diff) > 10.0f) {
                    e->touch.moveInput = glm::clamp(diff / 50.0f, -1.0f, 1.0f);
                    e->touch.moveInput.y = -e->touch.moveInput.y;
                }
                return 1;
            }

            // منطقة الكاميرا (اليمين)
            if (x >= (float)e->width * 0.45f && e->touch.lookPointerId == -1) {
                e->touch.lookPointerId = pointerId;
                e->touch.lastLookPos = glm::vec2(x, y);
                return 1;
            }
        }
        else if (actionMasked == AMOTION_EVENT_ACTION_MOVE) {
            size_t count = AMotionEvent_getPointerCount(ev);
            for (size_t i = 0; i < count; ++i) {
                int id = AMotionEvent_getPointerId(ev, i);
                float x = AMotionEvent_getX(ev, i);
                float y = AMotionEvent_getY(ev, i);

                if (id == e->touch.movePointerId) {
                    glm::vec2 diff = glm::vec2(x, y) - glm::vec2(dpadX, dpadY);
                    if (glm::length(diff) > 15.0f) {
                        e->touch.moveInput = glm::clamp(diff / 60.0f, -1.0f, 1.0f);
                        e->touch.moveInput.y = -e->touch.moveInput.y;
                    } else {
                        e->touch.moveInput = glm::vec2(0.0f);
                    }
                }

                if (id == e->touch.lookPointerId) {
                    float dx = x - e->touch.lastLookPos.x;
                    float dy = y - e->touch.lastLookPos.y;
                    e->touch.lastLookPos = glm::vec2(x, y);

                    e->player.yaw   += dx * 0.14f;
                    e->player.pitch -= dy * 0.14f;

                    if (e->player.pitch > 89.0f)  e->player.pitch = 89.0f;
                    if (e->player.pitch < -89.0f) e->player.pitch = -89.0f;
                }
            }
            return 1;
        }
        else if (actionMasked == AMOTION_EVENT_ACTION_UP || actionMasked == AMOTION_EVENT_ACTION_POINTER_UP) {
            if (pointerId == e->touch.movePointerId) {
                e->touch.movePointerId = -1;
                e->touch.moveInput = glm::vec2(0.0f);
            }
            if (pointerId == e->touch.lookPointerId) {
                e->touch.lookPointerId = -1;
            }
            if (pointerId == e->touch.jumpPointerId) {
                e->touch.jumpPointerId = -1;
                e->touch.jumpPressed = false;
            }
            return 1;
        }
    }
    return 0;
}

// -------------------------------------------------------------
// فيزياء اللاعب وحركته فوق البلوكات
// -------------------------------------------------------------
static void updatePlayer(Engine* e, float dt) {
    Player& p = e->player;

    glm::vec3 front;
    front.x = cos(glm::radians(p.yaw)) * cos(glm::radians(p.pitch));
    front.y = sin(glm::radians(p.pitch));
    front.z = sin(glm::radians(p.yaw)) * cos(glm::radians(p.pitch));
    front = glm::normalize(front);

    glm::vec3 walkForward = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
    glm::vec3 walkRight   = glm::normalize(glm::cross(walkForward, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::vec3 moveDir = walkForward * e->touch.moveInput.y + walkRight * e->touch.moveInput.x;
    if (glm::length(moveDir) > 0.05f) {
        moveDir = glm::normalize(moveDir);
        p.position += moveDir * p.speed * dt;
    }

    p.velocity.y += p.gravity * dt;
    p.position.y += p.velocity.y * dt;

    p.position.x = glm::clamp(p.position.x, 0.5f, 15.5f);
    p.position.z = glm::clamp(p.position.z, 0.5f, 15.5f);

    int bx = (int)std::floor(p.position.x);
    int bz = (int)std::floor(p.position.z);
    float groundHeight = 0.0f;

    if (e->chunk) {
        for (int y = 15; y >= 0; --y) {
            if (e->chunk->getBlock(bx, y, bz) != BlockType::Air) {
                groundHeight = (float)y + 1.0f;
                break;
            }
        }
    }

    if (p.position.y <= groundHeight) {
        p.position.y = groundHeight;
        p.velocity.y = 0.0f;
        p.isGrounded = true;
    }

    e->camera.position = p.position + glm::vec3(0.0f, p.eyeHeight, 0.0f);
    e->camera.front = front;
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

    glViewport(0, 0, e->width, e->height);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

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
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            int h = getTerrainHeight(x, z);
            for (int y = 0; y <= h; ++y) {
                if (y == h) e->chunk->setBlock(x, y, z, BlockType::Grass);
                else if (y >= h - 2) e->chunk->setBlock(x, y, z, BlockType::Dirt);
                else e->chunk->setBlock(x, y, z, BlockType::Stone);
            }
        }
    }

    int ty = getTerrainHeight(8, 8) + 1;
    for (int y = 0; y < 3; ++y) e->chunk->setBlock(8, ty + y, 8, BlockType::Dirt);
    for (int ox = -1; ox <= 1; ++ox)
        for (int oz = -1; oz <= 1; ++oz) {
            e->chunk->setBlock(8 + ox, ty + 2, 8 + oz, BlockType::OakLeaves);
            e->chunk->setBlock(8 + ox, ty + 3, 8 + oz, BlockType::OakLeaves);
        }
    e->chunk->setBlock(8, ty + 4, 8, BlockType::OakLeaves);
    e->chunk->buildMesh();

    e->player.position = glm::vec3(8.0f, (float)getTerrainHeight(8, 8) + 2.0f, 8.0f);
    e->lastFrameTime = std::chrono::steady_clock::now();

    return 0;
}

static void drawFrame(Engine* e) {
    if (e->display == EGL_NO_DISPLAY) return;

    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - e->lastFrameTime).count();
    e->lastFrameTime = now;
    if (dt > 0.05f) dt = 0.05f;

    updatePlayer(e, dt);

    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)e->width / (float)e->height;
    glm::mat4 proj = e->camera.getProjectionMatrix(aspect);
    glm::mat4 view = e->camera.getViewMatrix();

    // 1. رسم مجسم العالم 3D
    glUseProgram(e->blockShader);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D_ARRAY, e->texArray);
    glm::mat4 mChunk = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(e->blockShader, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(e->blockShader, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(e->blockShader, "uModel"), 1, GL_FALSE, &mChunk[0][0]);
    e->chunk->render();

    // 2. رسم الغيوم 3D
    glUseProgram(e->cloudShader);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    e->windOffset += 0.020f;
    if (e->windOffset > 3.5f * 24.0f) e->windOffset = 0.0f;
    glm::mat4 mCloud = glm::translate(glm::mat4(1.0f), glm::vec3(e->windOffset, 0.0f, 0.0f));
    glUniformMatrix4fv(glGetUniformLocation(e->cloudShader, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(e->cloudShader, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(e->cloudShader, "uModel"), 1, GL_FALSE, &mCloud[0][0]);
    glBindVertexArray(e->cloudVAO);
    glDrawArrays(GL_TRIANGLES, 0, e->cloudCount);
    glBindVertexArray(0);
    glDisable(GL_BLEND);

    // 3. رسم واجهة أزرار اللمس وعلامة التصويب
    renderAllUI(e);

    eglSwapBuffers(e->display, e->surface);
}

static void handleCmd(struct android_app* app, int32_t cmd) {
    auto* e = (Engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW && e->app->window) { 
        initDisplay(e); 
        e->animating = true; 
    }
    else if (cmd == APP_CMD_TERM_WINDOW) { e->animating = false; }
    else if (cmd == APP_CMD_GAINED_FOCUS) { 
        e->animating = true; 
        e->lastFrameTime = std::chrono::steady_clock::now();
    }
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
