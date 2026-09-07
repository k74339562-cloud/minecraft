#ifdef __ANDROID__

#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Shaders.hpp"
#include "TerrainGen.hpp"
#include "Clouds.hpp"
#include "TouchUI.hpp"
#include "ChunkSection.hpp"
#include "Camera.hpp"
#include "stb_image.h"

struct Engine {
    struct android_app* app;
    EGLDisplay display = EGL_NO_DISPLAY; EGLSurface surface = EGL_NO_SURFACE; EGLContext context = EGL_NO_CONTEXT;
    int32_t width = 0, height = 0; bool animating = false;
    GLuint blockShader = 0, cloudShader = 0, uiShader = 0, texArray = 0;
    ChunkSection* chunk = nullptr;
    CloudRenderer clouds;
    TouchUI ui;
    Camera camera;
    float rotAngle = 0.0f;
};

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
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    return id;
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
    glEnable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);

    e->blockShader = compileProgram(BLOCK_VERT_SRC, BLOCK_FRAG_SRC);
    e->cloudShader = compileProgram(CLOUD_VERT_SRC, CLOUD_FRAG_SRC);
    e->uiShader    = compileProgram(UI_VERT_SRC, UI_FRAG_SRC);

    e->texArray = loadTextures(e->app->activity->assetManager);
    e->clouds.init();
    e->ui.init(e->width);

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
    // شجرة
    int ty = getTerrainHeight(8, 8) + 1;
    for (int y = 0; y < 3; ++y) e->chunk->setBlock(8, ty + y, 8, BlockType::Dirt);
    for (int ox = -1; ox <= 1; ++ox)
        for (int oz = -1; oz <= 1; ++oz) {
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

    if (e->ui.isRotating) { e->rotAngle += 0.009f; e->clouds.windOffset += 0.025f; }

    // 1. رسم العالم
    glUseProgram(e->blockShader);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D_ARRAY, e->texArray);
    glm::mat4 mChunk = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(8,0,8)), e->rotAngle, glm::vec3(0,1,0));
    mChunk = glm::translate(mChunk, glm::vec3(-8,0,-8));
    glUniformMatrix4fv(glGetUniformLocation(e->blockShader, "uProjection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(e->blockShader, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(e->blockShader, "uModel"), 1, GL_FALSE, &mChunk[0][0]);
    e->chunk->render();

    // 2. رسم الغيوم
    glUseProgram(e->cloudShader); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glm::mat4 mCloud = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(8 + e->clouds.windOffset, 0, 8)), e->rotAngle * 0.3f, glm::vec3(0,1,0));
    mCloud = glm::translate(mCloud, glm::vec3(-8,0,-8));
    glUniformMatrix4fv(glGetUniformLocation(e->cloudShader, "uProjection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(e->cloudShader, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(e->cloudShader, "uModel"), 1, GL_FALSE, &mCloud[0][0]);
    e->clouds.render();
    glDisable(GL_BLEND);

    // 3. رسم الواجهة باللمس
    e->ui.render(e->uiShader, e->width, e->height);

    eglSwapBuffers(e->display, e->surface);
}

static int32_t handleInput(struct android_app* app, AInputEvent* ev) {
    auto* e = (Engine*)app->userData;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int act = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
        if (act == AMOTION_EVENT_ACTION_DOWN || act == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            float x = AMotionEvent_getX(ev, 0), y = AMotionEvent_getY(ev, 0);
            if (e->ui.handleTouch(x, y)) return 1;
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
