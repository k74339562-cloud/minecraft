cmake_minimum_required(VERSION 3.20)
project(MinecraftClone LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    include
    lib
    lib/glad
    lib/stb
)

# إذا كانت GLM موجودة
find_package(glm QUIET)
if (glm_FOUND)
    include_directories(${GLM_INCLUDE_DIRS})
else()
    include_directories(lib/glm)
endif()

# الملفات المشتركة بين الأندرويد والكمبيوتر
set(COMMON_SOURCES
    src/TextureArray.cpp
    src/Shader.cpp
    src/ChunkSection.cpp
)

# -------------------------------------------------------------
# في حال البناء للأندرويد (Android NDK)
# -------------------------------------------------------------
if (ANDROID)
    add_library(${PROJECT_NAME} SHARED
        ${COMMON_SOURCES}
        src/android_main.cpp
    )

    # تضمين مكتبات أندرويد الأساسية و EGL و GLESv3
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

# -------------------------------------------------------------
# في حال البناء للكمبيوتر المكتبي (Desktop PC)
# -------------------------------------------------------------
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
