cmake_minimum_required(VERSION 3.20)
project(MinecraftClone LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# مسارات الترويسات (Include Directories)
include_directories(
    include
    lib
    lib/glad
    lib/stb
)

# البحث عن GLFW و OpenGL
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

# إذا كانت GLM مثبتة على النظام ابحث عنها، وإلا استخدم الموجودة في lib
find_package(glm QUIET)
if (glm_FOUND)
    include_directories(${GLM_INCLUDE_DIRS})
else()
    include_directories(lib/glm)
endif()

# تجميع الملفات المصدرية
file(GLOB_RECURSE SOURCES 
    src/*.cpp 
    lib/glad/glad.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

# ربط المكتبات
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    OpenGL::GL 
    glfw
)
