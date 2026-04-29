#pragma once

#include <glm.hpp>

namespace Okari
{
    class Camera
    {
    public:
        Camera(float aspectRatio);

        glm::mat4 GetViewMatrix() const;
        glm::mat4 GetProjectionMatrix() const;

        void SetPosition(const glm::vec3& pos) { m_Position = pos; }
        void SetTarget(const glm::vec3& target) { m_Target = target; }

        glm::vec3 GetForward() const;
        glm::vec3 GetRight() const;

    private:
        glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 3.0f);
        glm::vec3 m_Target = glm::vec3(0.0f, 0.0f, 0.0f);

        float m_FOV = 45.0f;
        float m_AspectRatio;
        float m_Near = 0.1f;
        float m_Far = 100.0f;
    };
}