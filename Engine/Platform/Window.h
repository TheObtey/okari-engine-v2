#pragma once

#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Okari
{
    class Window
    {
    public:
        Window(int width, int height, const std::string& title);
        ~Window();

        void PollEvents();
        void SwapBuffers();

        bool ShouldClose() const;

        GLFWwindow* GetNativeWindow() const { return m_Window; }

    private:
        GLFWwindow* m_Window = nullptr;

        int m_Width = 0;
        int m_Height = 0;
        std::string m_Title;
    };
}