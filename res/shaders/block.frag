precision mediump float;
precision mediump sampler2DArray;

in vec2 vTexCoord;
flat in int vLayer;
in float vLight;

uniform sampler2DArray uTextureArray;

out vec4 FragColor;

void main() {
    // قراءة البكسل من مصفوفة التكستشر 2D Array
    vec4 texColor = texture(uTextureArray, vec3(vTexCoord, float(vLayer)));

    // خدعة الـ Cutout: إذا كان البكسل شفافاً (مثل فراغات أوراق الشجر)، الغِه فوراً!
    if (texColor.a < 0.5) {
        discard;
    }

    // تطبيق تظليل ماينكرافت الحسابي على ألوان البلوكة
    vec3 shadedColor = texColor.rgb * vLight;

    FragColor = vec4(shadedColor, texColor.a);
}
