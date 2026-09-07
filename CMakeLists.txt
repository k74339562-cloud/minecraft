cmake_minimum_required(VERSION 3.20)
project(MinecraftClone LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# مسارات الترويسات الشاملة
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
if (CMAKE_SYSTEM_NAME STREQUAL "Android" OR ANDROID)

    # تحديد مسار الـ NDK بشكل قاطع
    if (NOT DEFINED NDK_DIR)
        if (DEFINED ANDROID_NDK)
            set(NDK_DIR "${ANDROID_NDK}")
        elseif (DEFINED CMAKE_ANDROID_NDK)
            set(NDK_DIR "${CMAKE_ANDROID_NDK}")
        endif()
    endif()

    set(GLUE_DIR "${NDK_DIR}/sources/android/native_app_glue")
    set(GLUE_SRC "${GLUE_DIR}/android_native_app_glue.c")

    # تجميع اللعبة مع كود الربط مباشرة كـ Shared Library واحدة متماسكة
    add_library(${PROJECT_NAME} SHARED
        ${COMMON_SOURCES}
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
