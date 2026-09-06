// فك التشفير فائق السرعة داخل GPU
layout (location = 0) in uint aPackedData;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vTexCoord;
flat out int vLayer;
out float vLight;

// زوايا التكستشر الأربعة (UV)
const vec2 UV_CORNERS[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

// تظليل الاتجاهات لماينكرافت (Directional Lighting)
// 0: UP(+Y), 1: DOWN(-Y), 2: NORTH(-Z), 3: SOUTH(+Z), 4: WEST(-X), 5: EAST(+X)
const float FACE_LIGHT[6] = float[6](
    1.0,  // أعلى (أقوى إضاءة مباشرة من السماء)
    0.5,  // أسفل (معتم)
    0.8,  // شمال
    0.8,  // جنوب
    0.6,  // غرب
    0.6   // شرق
);

void main() {
    // فك تشفير البتات الـ 32 بعمليات Bitwise سريعة جداً
    uint x       = aPackedData & 31u;
    uint y       = (aPackedData >> 5u) & 31u;
    uint z       = (aPackedData >> 10u) & 31u;
    uint faceDir = (aPackedData >> 15u) & 7u;
    uint ao      = (aPackedData >> 18u) & 3u;
    uint layer   = (aPackedData >> 20u) & 1023u;
    uint uvIndex = (aPackedData >> 30u) & 3u;

    vec3 localPos = vec3(float(x), float(y), float(z));

    // إرسال البيانات للشيدر التالي
    vTexCoord = UV_CORNERS[uvIndex];
    vLayer = int(layer);
    vLight = FACE_LIGHT[faceDir]; // تظليل الوجه

    gl_Position = uProjection * uView * uModel * vec4(localPos, 1.0);
}
