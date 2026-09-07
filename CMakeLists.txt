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

# تجميع كل ملفات الـ C++ تلقائياً من مجلد src مهما كانت تسميتها
file(GLOB_RECURSE ALL_SOURCES "src/*.cpp")

if (NOT ALL_SOURCES)
    message(FATAL_ERROR "No .cpp source files found in 'src/'! Please make sure your source files are inside 'src/'.")
endif()

# ==============================================================================
# إعدادات البناء للأندرويد (Android NDK)
# ==============================================================================
if (CMAKE_SYSTEM_NAME STREQUAL "Android" OR ANDROID)

    # استبعاد main.cpp الخاص بالكمبيوتر
    set(ANDROID_SOURCES ${ALL_SOURCES})
    list(FILTER ANDROID_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

    # البحث التلقائي الدقيق عن ملف ربط الأندرويد داخل NDK
    find_file(GLUE_SRC
        NAMES android_native_app_glue.c
        PATHS
            "${ANDROID_NDK}/sources/android/native_app_glue"
            "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue"
            "$ENV{ANDROID_NDK_HOME}/sources/android/native_app_glue"
            "$ENV{ANDROID_NDK_ROOT}/sources/android/native_app_glue"
            "/usr/local/lib/android/sdk/ndk/26.1.10909125/sources/android/native_app_glue"
    )

    if (NOT GLUE_SRC)
        message(FATAL_ERROR "Could not find android_native_app_glue.c in Android NDK!")
    endif()

    get_filename_component(GLUE_DIR "${GLUE_SRC}" DIRECTORY)

    # تجميع اللعبة مع كود الربط مباشرة
    add_library(${PROJECT_NAME} SHARED
        ${ANDROID_SOURCES}
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
    set(DESKTOP_SOURCES ${ALL_SOURCES})
    list(FILTER DESKTOP_SOURCES EXCLUDE REGEX ".*/android_main\\.cpp$")

    find_package(OpenGL REQUIRED)
    find_package(glfw3 3.3 REQUIRED)

    add_executable(${PROJECT_NAME}
        ${DESKTOP_SOURCES}
        lib/glad/src/glad.c
    )

    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        OpenGL::GL
        glfw
    )
endif()
