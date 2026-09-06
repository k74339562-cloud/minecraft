#include "ChunkSection.hpp"
#include <cstring>

ChunkSection::ChunkSection(int chunkX, int chunkY, int chunkZ)
    : m_chunkX(chunkX), m_chunkY(chunkY), m_chunkZ(chunkZ) {
    std::memset(m_blocks, 0, sizeof(m_blocks)); // ملء المقطع بالهواء (Air = 0)
}

ChunkSection::~ChunkSection() {
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
}

void ChunkSection::setBlock(int x, int y, int z, BlockType type) {
    int index = x + (y * SECTION_SIZE) + (z * SECTION_SIZE * SECTION_SIZE);
    if (m_blocks[index] == BlockType::Air && type != BlockType::Air) {
        m_nonAirCount++;
    } else if (m_blocks[index] != BlockType::Air && type == BlockType::Air) {
        m_nonAirCount--;
    }
    m_blocks[index] = type;
}

BlockType ChunkSection::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= SECTION_SIZE || y < 0 || y >= SECTION_SIZE || z < 0 || z >= SECTION_SIZE) {
        return BlockType::Air; // مؤقتاً: نعتبر الحدود الخارجية هواء لحين ربط الـ Chunks المجاورة
    }
    return m_blocks[x + (y * SECTION_SIZE) + (z * SECTION_SIZE * SECTION_SIZE)];
}

// فحص حجب الوجه: إذا كان الجار صلباً، يتم إخفاء الوجه تماماً!
bool ChunkSection::isFaceVisible(int x, int y, int z, Direction dir) const {
    int nx = x, ny = y, nz = z;
    switch (dir) {
        case DIR_UP:    ny++; break;
        case DIR_DOWN:  ny--; break;
        case DIR_NORTH: nz--; break;
        case DIR_SOUTH: nz++; break;
        case DIR_WEST:  nx--; break;
        case DIR_EAST:  nx++; break;
    }

    // إذا خرج الجار عن حدود الـ Chunk نعتبره مرئياً حالياً
    if (nx < 0 || nx >= SECTION_SIZE || ny < 0 || ny >= SECTION_SIZE || nz < 0 || nz >= SECTION_SIZE) {
        return true; 
    }

    BlockType neighbor = getBlock(nx, ny, nz);
    return neighbor == BlockType::Air; // يظهر الوجه فقط إذا كان الجار هواء
}

void ChunkSection::addFace(std::vector<PackedVertex>& vertices, 
                           int x, int y, int z, Direction dir, int texLayer) {
    // كل وجه مربع يتكون من 6 رؤوس (مثلثين)
    // الإحداثيات المحلية للمكعب [0..1]
    uint32_t x0 = x, x1 = x + 1;
    uint32_t y0 = y, y1 = y + 1;
    uint32_t z0 = z, z1 = z + 1;
    uint32_t d = static_cast<uint32_t>(dir);
    uint32_t ao = 0; // الظل الافتراضي (سنفعله مع الرندر دراغون)

    // النقاط الأربع للوجه حسب الاتجاه
    switch (dir) {
        case DIR_UP: // +Y
            vertices.push_back(PackedVertex::create(x0, y1, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y1, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y1, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y1, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y1, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y1, z1, d, ao, texLayer));
            break;

        case DIR_DOWN: // -Y
            vertices.push_back(PackedVertex::create(x0, y0, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y0, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y0, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y0, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y0, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y0, z0, d, ao, texLayer));
            break;

        case DIR_NORTH: // -Z
            vertices.push_back(PackedVertex::create(x1, y0, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y0, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y1, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y1, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y1, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y0, z0, d, ao, texLayer));
            break;

        case DIR_SOUTH: // +Z
            vertices.push_back(PackedVertex::create(x0, y0, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y0, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y1, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y1, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y1, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y0, z1, d, ao, texLayer));
            break;

        case DIR_WEST: // -X
            vertices.push_back(PackedVertex::create(x0, y0, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y0, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y1, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y1, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y1, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x0, y0, z0, d, ao, texLayer));
            break;

        case DIR_EAST: // +X
            vertices.push_back(PackedVertex::create(x1, y0, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y0, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y1, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y1, z0, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y1, z1, d, ao, texLayer));
            vertices.push_back(PackedVertex::create(x1, y0, z1, d, ao, texLayer));
            break;
    }
}

void ChunkSection::buildMesh() {
    if (isEmpty()) return;

    std::vector<PackedVertex> vertices;
    vertices.reserve(4096); // حجز أولي لتجنب إعادة تخصيص الذاكرة

    for (int y = 0; y < SECTION_SIZE; ++y) {
        for (int z = 0; z < SECTION_SIZE; ++z) {
            for (int x = 0; x < SECTION_SIZE; ++x) {
                BlockType block = getBlock(x, y, z);
                if (block == BlockType::Air) continue;

                // أرقام التكستشر الافتراضية (سيربطها التكستشر مانيجر لاحقاً)
                int topTex = 0, sideTex = 1, bottomTex = 2;

                // فحص الأوجه الستة: Face Culling الصارم!
                if (isFaceVisible(x, y, z, DIR_UP))    addFace(vertices, x, y, z, DIR_UP, topTex);
                if (isFaceVisible(x, y, z, DIR_DOWN))  addFace(vertices, x, y, z, DIR_DOWN, bottomTex);
                if (isFaceVisible(x, y, z, DIR_NORTH)) addFace(vertices, x, y, z, DIR_NORTH, sideTex);
                if (isFaceVisible(x, y, z, DIR_SOUTH)) addFace(vertices, x, y, z, DIR_SOUTH, sideTex);
                if (isFaceVisible(x, y, z, DIR_WEST))  addFace(vertices, x, y, z, DIR_WEST, sideTex);
                if (isFaceVisible(x, y, z, DIR_EAST))  addFace(vertices, x, y, z, DIR_EAST, sideTex);
            }
        }
    }

    m_vertexCount = static_cast<GLsizei>(vertices.size());
    if (m_vertexCount == 0) return;

    // رفع المش المضغوط فوراً إلى VRAM كرت الشاشة
    if (m_vao == 0) glGenVertexArrays(1, &m_vao);
    if (m_vbo == 0) glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(PackedVertex), vertices.data(), GL_STATIC_DRAW);

    // إخبار OpenGL بأن البيانات عبارة عن عدد صحيح غير موقع بحجم 32 بت (Attribute واحد فقط!)
    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(PackedVertex), (void*)0);

    glBindVertexArray(0);
}

void ChunkSection::render() const {
    if (m_vertexCount == 0 || m_vao == 0) return;
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    glBindVertexArray(0);
}
