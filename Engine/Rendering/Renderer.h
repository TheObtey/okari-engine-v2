#pragma once

#include "Rendering/Shader.h"
#include "Rendering/Texture2D.h"
#include "Rendering/Camera.h"
#include "Rendering/Mesh.h"
#include "Rendering/DirectionalLight.h"
#include "Collision/CollisionTypes.h"
#include "Scene/Transform.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace Okari
{
    class Renderer
    {
    public:
        Renderer();
        ~Renderer();

        void Init();
        void BeginFrame();

        void SetTevDebugMode(int mode) { m_TevDebugMode = mode; }
        int GetTevDebugMode() const { return m_TevDebugMode; }

        void DrawCube(const Transform& transform, std::string& texturePath, const Camera& camera);
        void DrawCube(const glm::mat4& modelMatrix, std::string& texturePath, const Camera& camera);
        void DrawCubeOutline(const Transform& transform, const Camera& camera);
        void DrawCubeOutline(const glm::mat4& modelMatrix, const Camera& camera);
        void DrawCubeID(const Transform& transform, uint32_t objectID, const Camera& camera);
        void DrawCubeID(const glm::mat4& modelMatrix, uint32_t objectID, const Camera& camera);
        
        void DrawMesh(const Transform& transform, Mesh* mesh, std::string& texturePath, const Camera& camera, const DirectionalLight& light);
        void DrawMesh(const glm::mat4& modelMatrix, Mesh* mesh, std::string& texturePath, const Camera& camera, const DirectionalLight& light);
        
        void DrawMeshDebug(const glm::mat4& modelMatrix, Mesh* mesh, const Camera& camera, const glm::vec3& color);

        void DrawMeshOutline(const Transform& transform, Mesh* mesh, const Camera& camera);
        void DrawMeshOutline(const glm::mat4& modelMatrix, Mesh* mesh, const Camera& camera);
        
        void DrawCollisionMesh(const CollisionMesh& collisionMesh, const Camera& camera);
        void DrawCollisionMesh(const CollisionMesh& collisionMesh, const Camera& camera, const glm::mat4& modelMatrix);
        
        void EndFrame();

    private:
        unsigned int m_VAO = 0;
        unsigned int m_VBO = 0;

        std::unique_ptr<Shader> m_Shader;
        std::unique_ptr<Shader> m_OutlineShader;
        std::unique_ptr<Shader> m_PickingShader;
        std::unordered_map<std::string, std::unique_ptr<Texture2D>> m_TextureCache;

        Texture2D* GetTexture(const std::string& path);

        int m_TevDebugMode = 0;
    };
}