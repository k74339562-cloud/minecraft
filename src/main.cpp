#ifdef __ANDROID__

#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#define LOG_TAG "MinecraftClone"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

void android_main(struct android_app* state) {
    LOGI("Minecraft Native Engine Started on Android!");

    // حلقة الأحداث الأساسية لنظام أندرويد
    while (true) {
        int events;
        struct android_poll_source* source;

        while (ALooper_pollOnce(0, nullptr, &events, (void**)&source) >= 0) {
            if (source != nullptr) {
                source->process(state, source);
            }
            if (state->destroyRequested != 0) {
                LOGI("App Destroy Requested");
                return;
            }
        }

        // هنا ستعمل حلقة الرسم والشيدرات على شاشة الهاتف
    }
}

#endif
