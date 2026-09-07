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

find_package(glm QUIET)
if (glm_FOUND)
    include_directories(${GLM_INCLUDE_DIRS})
else()
    include_directories(lib/glm)
endif()

set(COMMON_SOURCES
    src/TextureArray.cpp
    src/Shader.cpp
    src/ChunkSection.cpp
)

if (ANDROID)
    # تجميع كود الربط الخاص بنظام أندرويد من داخل ملفات الـ NDK
    add_library(app-glue STATIC
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    )

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
