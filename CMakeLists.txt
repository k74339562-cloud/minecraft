cmake_minimum_required(VERSION 3.20)
project(MinecraftClone LANGUAGES C CXX)

# فرض استخدام معيار C++20 الحديث
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# المجلدات المشتركة لملفات الترويسات (Headers)
include_directories(
    include
    lib
    lib/glad
    lib/stb
)

# البحث عن مكتبة GLM أو استخدام المجلد الداخلي
find_package(glm QUIET)
if (glm_FOUND)
    include_directories(${GLM_INCLUDE_DIRS})
else()
    include_directories(lib/glm)
endif()

# الملفات البرمجية المشتركة (المحرك الأساسي)
set(COMMON_SOURCES
    src/TextureArray.cpp
    src/Shader.cpp
    src/ChunkSection.cpp
)

# ==============================================================================
# إعدادات البناء للأندرويد (Android NDK)
# ==============================================================================
if (ANDROID)
    # التأكد التام من الحصول على مسار الـ NDK الداخلي
    if (NOT DEFINED ANDROID_NDK)
        set(ANDROID_NDK ${CMAKE_ANDROID_NDK})
    endif()

    # تجميع كود الربط الخاص بـ NativeActivity من الـ NDK
    add_library(app-glue STATIC
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    )

    # إنشاء المكتبة الديناميكية الأساسية للعبة (.so)
    add_library(${PROJECT_NAME} SHARED
        ${COMMON_SOURCES}
        src/android_main.cpp
    )

    # مسار ترويسات android_native_app_glue
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${ANDROID_NDK}/sources/android/native_app_glue
    )

    # البحث عن مكتبات النظام في الأندرويد
    find_library(LOG_LIB log)
    find_library(ANDROID_LIB android)
    find_library(EGL_LIB EGL)
    find_library(GLES3_LIB GLESv3)

    # ربط المحرك بمكتبات الرسوميات والنظام
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        app-glue
        ${LOG_LIB}
        ${ANDROID_LIB}
        ${EGL_LIB}
        ${GLES3_LIB}
    )

# ==============================================================================
# إعدادات البناء للكمبيوتر (Desktop PC)
# ==============================================================================
else()
    find_package(OpenGL REQUIRED)
    find_package(glfw3 3.3 REQUIRED)

    add_executable(${PROJECT_NAME}
        ${COMMON_SOURCES}
        src/main.cpp
        lib/glad/glad.c
    )

    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        OpenGL::GL
        glfw
    )
endif()
