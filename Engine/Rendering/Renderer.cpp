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

        m_OutlineShader = std::make_unique<Shader>(
            "Assets/Shaders/outline.vert",
            "Assets/Shaders/outline.frag"
        );

        m_PickingShader = std::make_unique<Shader>(
            "Assets/Shaders/picking.vert",
            "Assets/Shaders/picking.frag"
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
    
    void Renderer::DrawCubeOutline(const Transform& transform, const Camera& camera)
    {
        m_OutlineShader->Bind();

        Transform outlineTransform = transform;
        //outlineTransform.Scale *= 1.03f;

        glm::mat4 model = outlineTransform.GetModelMatrix();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = camera.GetProjectionMatrix();

        glm::mat4 mvp = projection * view * model;

        m_OutlineShader->SetMat4("u_MVP", mvp);
        m_OutlineShader->SetVec3("u_Color", glm::vec3(1.0f, 0.85f, 0.05f));

        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(3.0f);

        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glLineWidth(1.0f);
    }

    void Renderer::DrawCubeID(const Transform& transform, uint32_t objectID, const Camera& camera)
    {
        m_PickingShader->Bind();

        glm::mat4 model = transform.GetModelMatrix();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = camera.GetProjectionMatrix();

        glm::mat4 mvp = projection * view * model;

        m_PickingShader->SetMat4("u_MVP", mvp);
        m_PickingShader->SetUInt("u_ObjectID", objectID);

        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
    }

    void Renderer::DrawMesh(const Transform& transform, Mesh* mesh, std::string& texturePath, const Camera& camera)
    {
        if (!mesh)
            return;

        m_Shader->Bind();

        glm::mat4 model = transform.GetModelMatrix();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = camera.GetProjectionMatrix();

        glm::mat4 mvp = projection * view * model;

        m_Shader->SetMat4("u_MVP", mvp);
        m_Shader->SetInt("u_Texture", 0);

        Texture2D* texture = GetTexture(texturePath);
        texture->Bind(0);

        mesh->Bind();
        glDrawArrays(GL_TRIANGLES, 0, mesh->GetVertexCount());
    }

    void Renderer::DrawMeshOutline(const Transform& transform, Mesh* mesh, const Camera& camera)
    {
        if (!mesh)
            return;

        m_OutlineShader->Bind();
        
        glm::mat4 model = transform.GetModelMatrix();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = camera.GetProjectionMatrix();
        glm::mat4 mvp = projection * view * model;

        m_OutlineShader->SetMat4("u_MVP", mvp);
        m_OutlineShader->SetVec3("u_Color", glm::vec3(1.0f, 0.85f, 0.05f));

        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(3.0f);

        mesh->Bind();
        
        glDrawArrays(GL_TRIANGLES, 0, mesh->GetVertexCount());

        glBindVertexArray(0);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glLineWidth(1.0f);
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