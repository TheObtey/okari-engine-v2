#include "Rendering/Renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

namespace Okari
{
    Renderer::Renderer()
    { }

    Renderer::~Renderer()
    {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
    }

    void Renderer::Init()
    {
        float vertices[] =
        {
            -0.5f,-0.5f,-0.5f, 0.0f, 0.0f,
             0.5f,-0.5f,-0.5f, 1.0f, 0.0f,
             0.5f, 0.5f,-0.5f, 1.0f, 1.0f,
             0.5f, 0.5f,-0.5f, 1.0f, 1.0f,
            -0.5f, 0.5f,-0.5f, 0.0f, 1.0f,
            -0.5f,-0.5f,-0.5f, 0.0f, 0.0f,

            -0.5f,-0.5f, 0.5f, 0.0f, 0.0f,
             0.5f,-0.5f, 0.5f, 1.0f, 0.0f,
             0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
             0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 1.0f,
            -0.5f,-0.5f, 0.5f, 0.0f, 0.0f,

            -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
            -0.5f, 0.5f,-0.5f, 1.0f, 1.0f,
            -0.5f,-0.5f,-0.5f, 0.0f, 1.0f,
            -0.5f,-0.5f,-0.5f, 0.0f, 1.0f,
            -0.5f,-0.5f, 0.5f, 0.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

             0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
             0.5f, 0.5f,-0.5f, 1.0f, 1.0f,
             0.5f,-0.5f,-0.5f, 0.0f, 1.0f,
             0.5f,-0.5f,-0.5f, 0.0f, 1.0f,
             0.5f,-0.5f, 0.5f, 0.0f, 0.0f,
             0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

            -0.5f,-0.5f,-0.5f, 0.0f, 1.0f,
             0.5f,-0.5f,-0.5f, 1.0f, 1.0f,
             0.5f,-0.5f, 0.5f, 1.0f, 0.0f,
             0.5f,-0.5f, 0.5f, 1.0f, 0.0f,
            -0.5f,-0.5f, 0.5f, 0.0f, 0.0f,
            -0.5f,-0.5f,-0.5f, 0.0f, 1.0f,

            -0.5f, 0.5f,-0.5f, 0.0f, 1.0f,
             0.5f, 0.5f,-0.5f, 1.0f, 1.0f,
             0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
             0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 0.0f,
            -0.5f, 0.5f,-0.5f, 0.0f, 1.0f
        };

        glEnable(GL_DEPTH_TEST);

        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);

        glBindVertexArray(m_VAO);

        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            5 * sizeof(float),
            (void*)0
        );

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            2,
            GL_FLOAT,
            GL_FALSE,
            5 * sizeof(float),
            (void*)(3 * sizeof(float))
        );

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        m_Shader = std::make_unique<Shader>(
            "Assets/Shaders/basic.vert",
            "Assets/Shaders/basic.frag"
        );
    }

    void Renderer::BeginFrame()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void Renderer::DrawCube(const Transform& transform, std::string& texturePath, const Camera& camera)
    {
        m_Shader->Bind();

        glm::mat4 model = transform.GetModelMatrix();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = camera.GetProjectionMatrix();

        glm::mat4 mvp = projection * view * model;

        m_Shader->SetMat4("u_MVP", mvp);
        m_Shader->SetInt("u_Texture", 0);

        Texture2D* texture = GetTexture(texturePath);
        texture->Bind(0);

        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
    }

    Texture2D* Renderer::GetTexture(const std::string& path)
    {
        auto it = m_TextureCache.find(path);

        if (it != m_TextureCache.end())
            return it->second.get();

        auto texture = std::make_unique<Texture2D>(path);
        Texture2D* texturePtr = texture.get();

        m_TextureCache[path] = std::move(texture);

        return texturePtr;
    }

    void Renderer::EndFrame()
    { }
}