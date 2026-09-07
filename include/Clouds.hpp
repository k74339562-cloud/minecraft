#pragma once
#include "OpenGL.hpp"
#include "TerrainGen.hpp"
#include <glm/glm.hpp>
#include <vector>

struct CloudVertex { glm::vec3 pos; glm::vec4 color; };

class CloudRenderer {
public:
    GLuint vao = 0, vbo = 0;
    GLsizei vertexCount = 0;
    float windOffset = 0.0f;

    void init() {
        std::vector<CloudVertex> v;
        constexpr int GRID = 24; constexpr float CELL = 3.5f, Y0 = 18.0f, Y1 = 19.8f;
        auto hasCloud = [](int x, int z) { return smoothNoise(x * 0.2f + 5.0f, z * 0.2f + 5.0f) > 0.40f; };
        glm::vec4 cTop(1.0f, 1.0f, 1.0f, 0.85f), cBot(0.72f, 0.75f, 0.82f, 0.8f), cSide(0.85f, 0.88f, 0.92f, 0.82f);
        float off = (GRID * CELL) * 0.5f;

        for (int z = 0; z < GRID; ++z) {
            for (int x = 0; x < GRID; ++x) {
                if (!hasCloud(x, z)) continue;
                float x0 = x * CELL - off, x1 = x0 + CELL, z0 = z * CELL - off, z1 = z0 + CELL;
                v.push_back({{x0,Y0,z0}, cBot}); v.push_back({{x1,Y0,z0}, cBot}); v.push_back({{x1,Y0,z1}, cBot});
                v.push_back({{x1,Y0,z1}, cBot}); v.push_back({{x0,Y0,z1}, cBot}); v.push_back({{x0,Y0,z0}, cBot});
                v.push_back({{x0,Y1,z1}, cTop}); v.push_back({{x1,Y1,z1}, cTop}); v.push_back({{x1,Y1,z0}, cTop});
                v.push_back({{x1,Y1,z0}, cTop}); v.push_back({{x0,Y1,z0}, cTop}); v.push_back({{x0,Y1,z1}, cTop});
                if (z==0 || !hasCloud(x, z-1)) { v.push_back({{x1,Y0,z0}, cSide}); v.push_back({{x0,Y0,z0}, cSide}); v.push_back({{x0,Y1,z0}, cSide}); v.push_back({{x0,Y1,z0}, cSide}); v.push_back({{x1,Y1,z0}, cSide}); v.push_back({{x1,Y0,z0}, cSide}); }
                if (z==GRID-1 || !hasCloud(x, z+1)) { v.push_back({{x0,Y0,z1}, cSide}); v.push_back({{x1,Y0,z1}, cSide}); v.push_back({{x1,Y1,z1}, cSide}); v.push_back({{x1,Y1,z1}, cSide}); v.push_back({{x0,Y1,z1}, cSide}); v.push_back({{x0,Y0,z1}, cSide}); }
                if (x==0 || !hasCloud(x-1, z)) { v.push_back({{x0,Y0,z0}, cSide}); v.push_back({{x0,Y0,z1}, cSide}); v.push_back({{x0,Y1,z1}, cSide}); v.push_back({{x0,Y1,z1}, cSide}); v.push_back({{x0,Y1,z0}, cSide}); v.push_back({{x0,Y0,z0}, cSide}); }
                if (x==GRID-1 || !hasCloud(x+1, z)) { v.push_back({{x1,Y0,z1}, cSide}); v.push_back({{x1,Y0,z0}, cSide}); v.push_back({{x1,Y1,z0}, cSide}); v.push_back({{x1,Y1,z0}, cSide}); v.push_back({{x1,Y1,z1}, cSide}); v.push_back({{x1,Y0,z1}, cSide}); }
            }
        }
        vertexCount = (GLsizei)v.size();
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(CloudVertex), v.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CloudVertex), (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CloudVertex), (void*)12);
        glBindVertexArray(0);
    }

    void render() const {
        if (vao == 0) return;
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        glBindVertexArray(0);
    }
};
