#pragma once

#include <cstdint>
#include <string>
#include <vector>

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
}