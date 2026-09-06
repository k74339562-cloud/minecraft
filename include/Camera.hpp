#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    glm::vec3 position{8.0f, 10.0f, 25.0f};
    glm::vec3 front{0.0f, -0.3f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    glm::mat4 getProjectionMatrix(float aspectRatio) const {
        return glm::perspective(glm::radians(70.0f), aspectRatio, 0.1f, 1000.0f);
    }
};
