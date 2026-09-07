cmake_minimum_required(VERSION 3.20)
project(MinecraftClone LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    include
    lib
    lib/glad/include
    lib/glad
    lib/stb
    lib/glm
)

# ملفات المحرك الأساسية
set(ENGINE_SOURCES
    src/TextureArray.cpp
    src/Shader.cpp
    src/ChunkSection.cpp
)

# ==============================================================================
# إعدادات البناء للأندرويد (Android NDK)
# ==============================================================================
if (CMAKE_SYSTEM_NAME STREQUAL "Android" OR ANDROID)

    find_file(GLUE_SRC
        NAMES android_native_app_glue.c
        PATHS
            "${ANDROID_NDK}/sources/android/native_app_glue"
            "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue"
            "$ENV{ANDROID_NDK_HOME}/sources/android/native_app_glue"
            "$ENV{ANDROID_NDK_ROOT}/sources/android/native_app_glue"
            "/usr/local/lib/android/sdk/ndk/26.1.10909125/sources/android/native_app_glue"
    )

    get_filename_component(GLUE_DIR "${GLUE_SRC}" DIRECTORY)

    # تجميع ملفات المحرك مع android_main مباشرة
    add_library(${PROJECT_NAME} SHARED
        ${ENGINE_SOURCES}
        src/android_main.cpp
        ${GLUE_SRC}
    )

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${GLUE_DIR}
        include
    )

    find_library(LOG_LIB log)
    find_library(ANDROID_LIB android)
    find_library(EGL_LIB EGL)
    find_library(GLES3_LIB GLESv3)

    target_link_libraries(${PROJECT_NAME}
        PRIVATE
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
        ${ENGINE_SOURCES}
        src/main.cpp
        lib/glad/src/glad.c
    )

    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        OpenGL::GL
        glfw
    )
endif()
