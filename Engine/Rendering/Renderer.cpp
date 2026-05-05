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
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

#pragma region GLENUM_HELPERS
    static GLenum ToGLCullFace(CullMode mode)
    {
        switch (mode)
        {
        case CullMode::Front:
            return GL_FRONT;

        case CullMode::Back:
        default:
            return GL_BACK;
        }
    }

    static GLenum ToGLDepthFunc(const std::string& func)
    {
        if (func == "less")
            return GL_LESS;

        if (func == "equal")
            return GL_EQUAL;

        if (func == "greater")
            return GL_GREATER;

        if (func == "gequal")
            return GL_GEQUAL;

        if (func == "always")
            return GL_ALWAYS;

        if (func == "never")
            return GL_NEVER;

        if (func == "nequal")
            return GL_NOTEQUAL;

        return GL_LEQUAL;
    }

    static GLenum ToGLBlendFactor(const std::string& factor)
    {
        if (factor == "zero")
            return GL_ZERO;

        if (factor == "one")
            return GL_ONE;

        if (factor == "src_color")
            return GL_SRC_COLOR;

        if (factor == "inv_src_color")
            return GL_ONE_MINUS_SRC_COLOR;

        if (factor == "src_alpha")
            return GL_SRC_ALPHA;

        if (factor == "inv_src_alpha")
            return GL_ONE_MINUS_SRC_ALPHA;

        if (factor == "dst_alpha")
            return GL_DST_ALPHA;

        if (factor == "inv_dst_alpha")
            return GL_ONE_MINUS_DST_ALPHA;

        return GL_ONE;
    }
#pragma endregion

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

    void Renderer::DrawMesh(const Transform& transform, Mesh* mesh, std::string& texturePath, const Camera& camera, const DirectionalLight& light)
    {
        if (!mesh)
            return;

        m_Shader->Bind();

        glm::mat4 model = transform.GetModelMatrix();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = camera.GetProjectionMatrix();
        glm::mat4 mvp = projection * view * model;

        m_Shader->SetMat4("u_MVP", mvp);
        m_Shader->SetMat4("u_Model", model);

        m_Shader->SetVec3("u_LightDirection", light.Direction);
        m_Shader->SetVec3("u_LightColor", light.Color);
        m_Shader->SetVec3("u_AmbiantColor", light.Ambiant);

        m_Shader->SetInt("u_Texture", 0);

        mesh->Bind();

        const auto& subMeshes = mesh->GetSubMeshes();
        const auto& materials = mesh->GetMaterials();

        for (const SubMesh& subMesh : subMeshes)
        {
            std::string finalTexturePath = texturePath;

            AlphaMode alphaMode = AlphaMode::Opaque;
            float alphaCutoff = 0.5f;

            bool blendEnabled = false;
            std::string blendSrc = "one";
            std::string blendDst = "zero";

            CullMode cullMode = CullMode::Back;

            bool depthTest = true;
            bool depthWrite = true;
            std::string depthFunc = "lequal";

            if (subMesh.MaterialIndex < materials.size())
            {
                const Material& material = materials[subMesh.MaterialIndex];

                if (!material.DiffuseTexturePath.empty())
                    finalTexturePath = material.DiffuseTexturePath;

                alphaMode = material.Alpha;
                alphaCutoff = material.AlphaCutoff;

                blendEnabled = material.BlendEnabled;
                blendSrc = material.BlendSrc;
                blendDst = material.BlendDst;

                cullMode = material.Culling;

                depthTest = material.DepthTest;
                depthWrite = material.DepthWrite;
                depthFunc = material.DepthFunc;
            }

            m_Shader->SetInt("u_UseAlphaCutout", alphaMode == AlphaMode::Cutout ? 1 : 0);

            if (depthTest)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);

            glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);
            glDepthFunc(ToGLDepthFunc(depthFunc));

            if (cullMode == CullMode::None)
            {
                glDisable(GL_CULL_FACE);
            }
            else
            {
                glEnable(GL_CULL_FACE);
                glCullFace(ToGLCullFace(cullMode));
            }

            glDisable(GL_BLEND);

            Texture2D* texture = GetTexture(finalTexturePath);
            texture->Bind(0);

            glDrawArrays(
                GL_TRIANGLES,
                subMesh.VertexOffset,
                subMesh.VertexCount
            );
        }

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glBindVertexArray(0);
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