#pragma once
#include "OpenGL.hpp"

inline const char* BLOCK_VERT_SRC = R"(#version 300 es
layout (location = 0) in uint aPackedData;
uniform mat4 uModel, uView, uProjection;
out vec2 vTexCoord; flat out int vLayer; out float vLight;

const vec2 UV_CORNERS[4] = vec2[4](vec2(0,0), vec2(1,0), vec2(1,1), vec2(0,1));
const float FACE_LIGHT[6] = float[6](1.0, 0.5, 0.8, 0.8, 0.6, 0.6);

void main() {
    uint x = aPackedData & 31u, y = (aPackedData >> 5u) & 31u, z = (aPackedData >> 10u) & 31u;
    uint faceDir = (aPackedData >> 15u) & 7u, layer = (aPackedData >> 20u) & 1023u, uvIndex = (aPackedData >> 30u) & 3u;
    vTexCoord = UV_CORNERS[uvIndex]; vLayer = int(layer); vLight = FACE_LIGHT[faceDir];
    gl_Position = uProjection * uView * uModel * vec4(float(x), float(y), float(z), 1.0);
}
)";

inline const char* BLOCK_FRAG_SRC = R"(#version 300 es
precision mediump float; precision mediump sampler2DArray;
in vec2 vTexCoord; flat in int vLayer; in float vLight;
uniform sampler2DArray uTextureArray;
out vec4 FragColor;

void main() {
    vec4 tex = texture(uTextureArray, vec3(vTexCoord, float(vLayer)));
    if (tex.a < 0.5) discard;
    FragColor = vec4(tex.rgb * vLight, tex.a);
}
)";

inline const char* CLOUD_VERT_SRC = R"(#version 300 es
layout (location = 0) in vec3 aPos; layout (location = 1) in vec4 aColor;
uniform mat4 uModel, uView, uProjection;
out vec4 vColor;
void main() { vColor = aColor; gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0); }
)";

inline const char* CLOUD_FRAG_SRC = R"(#version 300 es
precision mediump float; in vec4 vColor; out vec4 FragColor;
void main() { FragColor = vColor; }
)";

inline const char* UI_VERT_SRC = R"(#version 300 es
layout (location = 0) in vec2 aPos; layout (location = 1) in vec4 aColor;
uniform mat4 uOrtho; out vec4 vColor;
void main() { vColor = aColor; gl_Position = uOrtho * vec4(aPos, 0.0, 1.0); }
)";

inline const char* UI_FRAG_SRC = R"(#version 300 es
precision mediump float; in vec4 vColor; out vec4 FragColor;
void main() { FragColor = vColor; }
)";

inline GLuint compileProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, nullptr); glCompileShader(fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}
