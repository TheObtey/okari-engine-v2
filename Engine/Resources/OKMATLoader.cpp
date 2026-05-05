#include "OKMATLoader.h"

#include <json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace Okari
{
	static AlphaMode ParseAlphaMode(const std::string& value)
	{
		if (value == "cutout")
			return AlphaMode::Cutout;

		if (value == "blend")
			return AlphaMode::Blend;

		return AlphaMode::Opaque;
	}

	static CullMode ParseCullMode(const std::string& value)
	{
		if (value == "none")
			return CullMode::None;

		if (value == "front")
			return CullMode::Front;

		return CullMode::Back;
	}

	static std::string ResolveTexturePath(const std::filesystem::path& okmatPath, const std::string& textureName)
	{
		if (textureName.empty())
			return "";

		std::filesystem::path baseDir = okmatPath.parent_path();
		std::filesystem::path texturePath = baseDir / (textureName + ".png");

		return texturePath.generic_string();
	}

	static bool EndsWith(const std::string& value, const std::string& suffix)
	{
		if (suffix.size() > value.size())
			return false;

		return std::equal(
			suffix.rbegin(),
			suffix.rend(),
			value.rbegin()
		);
	}

	static bool MaterialNameMatches(const std::string& meshName, const std::string& okmatName)
	{
		if (meshName == okmatName)
			return true;

		return EndsWith(meshName, okmatName);
	}

	bool OKMATLoader::ApplyToMesh(Mesh* mesh, const std::string& okmatPath)
	{
		if (!mesh)
			return false;

		std::ifstream file(okmatPath);

		if (!file.is_open())
		{
			std::cout << "[OKMATLoader] No OKMAT found: " << okmatPath << std::endl;
			return false;
		}

		nlohmann::json data;
		file >> data;

		if (!data.contains("materials") || !data["materials"].is_array())
		{
			std::cerr << "[OKMATLoader] Invalid OKMAT, missing materials array: " << okmatPath << std::endl;
			return false;
		}

		std::vector<Material>& meshMaterials = mesh->GetMaterials();
		std::filesystem::path okmatFsPath(okmatPath);

		for (const auto& okmatMaterial : data["materials"])
		{
			if (!okmatMaterial.contains("name"))
				continue;

			std::string materialName = okmatMaterial["name"].get<std::string>();

			for (Material& material : meshMaterials)
			{
				if (!MaterialNameMatches(material.Name, materialName))
					continue;

				const std::string originalDiffuseTexturePath = material.DiffuseTexturePath;

				if (okmatMaterial.contains("alpha_mode"))
				{
					material.Alpha = ParseAlphaMode(okmatMaterial["alpha_mode"].get<std::string>());
					material.UseAlphaCutout = material.Alpha == AlphaMode::Cutout;
				}

				if (okmatMaterial.contains("alpha_cutoff"))
					material.AlphaCutoff = okmatMaterial["alpha_cutoff"].get<float>();

				if (okmatMaterial.contains("culling"))
					material.Culling = ParseCullMode(okmatMaterial["culling"].get<std::string>());

				if (okmatMaterial.contains("depth"))
				{
					const auto& depth = okmatMaterial["depth"];

					if (depth.contains("test"))
						material.DepthTest = depth["test"].get<bool>();

					if (depth.contains("write"))
						material.DepthWrite = depth["write"].get<bool>();

					if (depth.contains("func"))
						material.DepthFunc = depth["func"].get<std::string>();
				}

				if (okmatMaterial.contains("render_queue"))
				{
					const std::string queue = okmatMaterial["render_queue"].get<std::string>();

					if (queue == "cutout")
						material.RenderQueue = 1;
					else if (queue == "transparent")
						material.RenderQueue = 2;
					else
						material.RenderQueue = 0;
				}

				if (okmatMaterial.contains("blend"))
				{
					const auto& blend = okmatMaterial["blend"];

					if (blend.contains("enabled"))
						material.BlendEnabled = blend["enabled"].get<bool>();

					if (blend.contains("type"))
						material.BlendType = blend["type"].get<std::string>();

					if (blend.contains("src"))
						material.BlendSrc = blend["src"].get<std::string>();

					if (blend.contains("dst"))
						material.BlendDst = blend["dst"].get<std::string>();

					if (blend.contains("logic"))
						material.BlendLogic = blend["logic"].get<std::string>();
				}

				if (okmatMaterial.contains("textures") && okmatMaterial["textures"].is_array())
				{
					material.TextureSlots.clear();

					for (const auto& texture : okmatMaterial["textures"])
					{
						MaterialTextureSlot slot;

						if (texture.contains("slot"))
							slot.Slot = texture["slot"].get<uint32_t>();

						if (texture.contains("index"))
							slot.Index = texture["index"].get<uint32_t>();

						if (texture.contains("name"))
							slot.Name = texture["name"].get<std::string>();

						slot.Path = ResolveTexturePath(okmatFsPath, slot.Name);

						material.TextureSlots.push_back(slot);
					}
				}

				material.DiffuseTexturePath = originalDiffuseTexturePath;

				std::cout << "[OKMATLoader] Applied material: " << material.Name
					<< " | alpha=" << static_cast<int>(material.Alpha)
					<< " | blend=" << material.BlendEnabled
					<< " | queue=" << material.RenderQueue
					<< std::endl;

				break;
			}
		}

		return true;
	}
}