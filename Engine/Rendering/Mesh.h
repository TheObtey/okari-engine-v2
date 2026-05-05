#pragma once

#include <glad/glad.h>
#include <glm.hpp>
#include <vector>
#include <string>

namespace Okari
{
	enum class AlphaMode
	{
		Opaque,
		Cutout,
		Blend
	};

	enum class CullMode
	{
		None,
		Back,
		Front
	};

	struct AABB
	{
		glm::vec3 Min;
		glm::vec3 Max;
	};

	struct Vertex
	{
		float Position[3];
		float UV[2];
		float Normal[3];
	};

	struct MaterialTextureSlot
	{
		uint32_t Slot = 0;
		uint32_t Index = 0;
		std::string Name;
		std::string Path;
	};

	struct Material
	{
		std::string Name;

		std::string DiffuseTexturePath;
		std::vector<MaterialTextureSlot> TextureSlots;

		AlphaMode Alpha = AlphaMode::Opaque;
		float AlphaCutoff = 0.5f;

		bool BlendEnabled = false;

		std::string BlendType = "none";
		std::string BlendSrc = "one";
		std::string BlendDst = "zero";
		std::string BlendLogic = "copy";

		CullMode Culling = CullMode::Back;

		bool DepthTest = true;
		bool DepthWrite = true;

		std::string DepthFunc = "lequal";

		uint32_t RenderQueue = 0;

		bool UseAlphaCutout = false;
	};

	struct SubMesh
	{
		uint32_t VertexOffset = 0;
		uint32_t VertexCount = 0;
		uint32_t MaterialIndex = 0;
	};

	class Mesh
	{
	public:
		Mesh(
			const std::vector<Vertex>& vertices,
			const std::vector<SubMesh>& subMeshes = {},
			const std::vector<Material>& materials = {}
		);

		~Mesh();

		void Bind() const;
		
		uint32_t GetVertexCount() const;
		const AABB& GetBounds() const { return m_Bounds; };

		const std::vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }
		const std::vector<Material>& GetMaterials() const { return m_Materials; }
		std::vector<Material>& GetMaterials() { return m_Materials; }

	private:
		uint32_t m_VAO = 0;
		uint32_t m_VBO = 0;
		uint32_t m_VertexCount = 0;

		AABB m_Bounds;

		std::vector<SubMesh> m_SubMeshes;
		std::vector<Material> m_Materials;
	};
}