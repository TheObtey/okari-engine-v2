#include "Core/Time.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Okari
{
    float Time::s_DeltaTime = 0.0f;
    float Time::s_Time = 0.0f;
    float Time::s_LastFrameTime = 0.0f;

    void Time::Update()
    {
        float currentTime = (float)glfwGetTime();

        s_DeltaTime = currentTime - s_LastFrameTime;
        s_LastFrameTime = currentTime;
        s_Time = currentTime;
    }
}