#include "Rendering/Camera.h"
#include <gtc/matrix_transform.hpp>

namespace Okari
{
    Camera::Camera(float aspectRatio)
        : m_AspectRatio(aspectRatio)
    { }

    glm::mat4 Camera::GetViewMatrix() const
    {
        return glm::lookAt(
            m_Position,
            m_Target,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
    }

    glm::mat4 Camera::GetProjectionMatrix() const
    {
        return glm::perspective(
            glm::radians(m_FOV),
            m_AspectRatio,
            m_Near,
            m_Far
        );
    }

    glm::vec3 Camera::GetForward() const
    {
        return glm::normalize(m_Target - m_Position);
    }

    glm::vec3 Camera::GetRight() const
    {
        return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }
}