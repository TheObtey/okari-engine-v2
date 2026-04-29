#pragma once

#include "Rendering/Shader.h"
#include "Rendering/Texture2D.h"
#include "Rendering/Camera.h"
#include "Scene/Transform.h"
#include "Scene/WorldObject.h"
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
        void DrawCube(const Transform& transform, std::string& texturePath, const Camera& camera);
        void EndFrame();

    private:
        unsigned int m_VAO = 0;
        unsigned int m_VBO = 0;

        std::unique_ptr<Shader> m_Shader;
        std::unordered_map<std::string, std::unique_ptr<Texture2D>> m_TextureCache;

        Texture2D* GetTexture(const std::string& path);
    };
}