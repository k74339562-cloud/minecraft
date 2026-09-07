#pragma once

#ifdef __ANDROID__
    #include <GLES3/gl3.h>
    #include <EGL/egl.h>
#else
    #include <glad/glad.h>
#endif
