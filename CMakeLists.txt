cmake_minimum_required(VERSION 3.20)
project(MinecraftClone LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# مسارات الترويسات الشاملة لكل المكتبات
include_directories(
    include
    lib
    lib/glad/include
    lib/glad
    lib/stb
    lib/glm
)

# الملفات المشتركة للمحرك
set(COMMON_SOURCES
    src/TextureArray.cpp
    src/Shader.cpp
    src/ChunkSection.cpp
)

# ==============================================================================
# إعدادات البناء للأندرويد (Android NDK)
# ==============================================================================
if (ANDROID)
    if (NOT DEFINED ANDROID_NDK)
        set(ANDROID_NDK ${CMAKE_ANDROID_NDK})
    endif()

    # تجميع كود ربط أندرويد الداخلي مع مساراته
    add_library(app-glue STATIC
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    )
    target_include_directories(app-glue PUBLIC
        ${ANDROID_NDK}/sources/android/native_app_glue
    )

    # تجميع ملفات اللعبة كـ Native Library للأندرويد
    add_library(${PROJECT_NAME} SHARED
        ${COMMON_SOURCES}
        src/android_main.cpp
    )

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${ANDROID_NDK}/sources/android/native_app_glue
    )

    find_library(LOG_LIB log)
    find_library(ANDROID_LIB android)
    find_library(EGL_LIB EGL)
    find_library(GLES3_LIB GLESv3)

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
        lib/glad/src/glad.c
    )

    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        OpenGL::GL
        glfw
    )
endif()
