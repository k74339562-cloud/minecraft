#pragma once
#include "OpenGL.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

struct UIVertex { glm::vec2 pos; glm::vec4 color; };

class TouchUI {
public:
    GLuint vao = 0, vbo = 0;
    float btnX = 0, btnY = 0, btnW = 100, btnH = 100;
    bool isRotating = true;

    void init(int width) {
        btnW = btnH = 100.0f;
        btnX = (float)width - btnW - 50.0f;
        btnY = 40.0f;

        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)8);
        glBindVertexArray(0);
    }

    bool handleTouch(float tx, float ty) {
        if (tx >= btnX && tx <= btnX + btnW && ty >= btnY && ty <= btnY + btnH) {
            isRotating = !isRotating;
            return true;
        }
        return false;
    }

    void render(GLuint shader, int screenW, int screenH) {
        glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(shader);

        glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f, -1.0f, 1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader, "uOrtho"), 1, GL_FALSE, &ortho[0][0]);

        std::vector<UIVertex> v;
        auto addQuad = [&](float x0, float y0, float x1, float y1, glm::vec4 c) {
            v.push_back({{x0,y0},c}); v.push_back({{x1,y0},c}); v.push_back({{x1,y1},c});
            v.push_back({{x1,y1},c}); v.push_back({{x0,y1},c}); v.push_back({{x0,y0},c});
        };

        // خلفية وإطار الزر
        addQuad(btnX, btnY, btnX + btnW, btnY + btnH, {0.12f, 0.12f, 0.14f, 0.75f});
        addQuad(btnX, btnY, btnX + btnW, btnY + 4, {0.5f, 0.5f, 0.55f, 0.85f});
        addQuad(btnX, btnY, btnX + 4, btnY + btnH, {0.5f, 0.5f, 0.55f, 0.85f});
        addQuad(btnX, btnY + btnH - 4, btnX + btnW, btnY + btnH, {0.05f, 0.05f, 0.08f, 0.9f});
        addQuad(btnX + btnW - 4, btnY, btnX + btnW, btnY + btnH, {0.05f, 0.05f, 0.08f, 0.9f});

        // أيقونة || أو ▶
        if (isRotating) {
            addQuad(btnX + 30, btnY + 25, btnX + 42, btnY + btnH - 25, {1,1,1,0.95f});
            addQuad(btnX + 58, btnY + 25, btnX + 70, btnY + btnH - 25, {1,1,1,0.95f});
        } else {
            v.push_back({{btnX + 35, btnY + 25}, {0.3f, 0.9f, 0.3f, 1}});
            v.push_back({{btnX + 75, btnY + btnH * 0.5f}, {0.3f, 0.9f, 0.3f, 1}});
            v.push_back({{btnX + 35, btnY + btnH - 25}, {0.3f, 0.9f, 0.3f, 1}});
        }

        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(UIVertex), v.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)v.size());
        glBindVertexArray(0); glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    }
};
