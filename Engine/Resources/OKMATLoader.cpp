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

	static glm::vec4 ParseVec4(const nlohmann::json& value)
	{
		if (!value.is_array() || value.size() < 4)
			return glm::vec4(1.0f);

		return glm::vec4(
			value[0].get<float>(),
			value[1].get<float>(),
			value[2].get<float>(),
			value[3].get<float>()
		);
	}

	static GXTevRegister ParseTevRegister(const std::string& value)
	{
		if (value == "reg0")
			return GXTevRegister::Reg0;
		if (value == "reg1")
			return GXTevRegister::Reg1;
		if (value == "reg2")
			return GXTevRegister::Reg2;

		return GXTevRegister::Prev;
	}

	static GXTevColorArg ParseTevArg(const std::string& value)
	{
		if (value == "one")
			return GXTevColorArg::One;

		if (value == "tex_color" || value == "texc")
			return GXTevColorArg::TexColor;
		if (value == "tex_alpha" || value == "texa")
			return GXTevColorArg::TexAlpha;

		if (value == "ras_color" || value == "rasc")
			return GXTevColorArg::RasColor;
		if (value == "ras_alpha" || value == "rasa")
			return GXTevColorArg::RasAlpha;

		if (value == "konst_color" || value == "konstc")
			return GXTevColorArg::KonstColor;
		if (value == "konst_alpha" || value == "konsta")
			return GXTevColorArg::KonstAlpha;

		if (value == "prev_color" || value == "prev")
			return GXTevColorArg::PrevColor;
		if (value == "prev_alpha")
			return GXTevColorArg::PrevAlpha;

		if (value == "reg0_color")
			return GXTevColorArg::Reg0Color;
		if (value == "reg0_alpha")
			return GXTevColorArg::Reg0Alpha;

		if (value == "reg1_color")
			return GXTevColorArg::Reg1Color;
		if (value == "reg1_alpha")
			return GXTevColorArg::Reg1Alpha;

		if (value == "reg2_color")
			return GXTevColorArg::Reg2Color;
		if (value == "reg2_alpha")
			return GXTevColorArg::Reg2Alpha;

		return GXTevColorArg::Zero;
	}

	static GXTevOp ParseTevOp(const std::string& value)
	{
		if (value == "sub" || value == "subtract")
			return GXTevOp::Subtract;

		return GXTevOp::Add;
	}

	static void ParseTevColorStage(const nlohmann::json& json, GXTevColorStage& stage)
	{					
		if (json.contains("a"))
			stage.A = ParseTevArg(json["a"].get<std::string>());
		if (json.contains("b"))
			stage.B = ParseTevArg(json["b"].get<std::string>());
		if (json.contains("c"))
			stage.C = ParseTevArg(json["c"].get<std::string>());
		if (json.contains("d"))
			stage.D = ParseTevArg(json["d"].get<std::string>());

		if (json.contains("op"))
			stage.Operation = ParseTevOp(json["op"].get<std::string>());
		if (json.contains("operation"))
			stage.Operation = ParseTevOp(json["operation"].get<std::string>());

		if (json.contains("output"))
			stage.Output = ParseTevRegister(json["output"].get<std::string>());
		if (json.contains("dest"))
			stage.Output = ParseTevRegister(json["dest"].get<std::string>());
	}

	static void ParseTevAlphaStage(const nlohmann::json& json, GXTevAlphaStage& stage)
	{
		if (json.contains("a"))
			stage.A = ParseTevArg(json["a"].get<std::string>());
		if (json.contains("b"))
			stage.B = ParseTevArg(json["b"].get<std::string>());
		if (json.contains("c"))
			stage.C = ParseTevArg(json["c"].get<std::string>());
		if (json.contains("d"))
			stage.D = ParseTevArg(json["d"].get<std::string>());

		if (json.contains("op"))
			stage.Operation = ParseTevOp(json["op"].get<std::string>());
		if (json.contains("operation"))
			stage.Operation = ParseTevOp(json["operation"].get<std::string>());

		if (json.contains("output"))
			stage.Output = ParseTevRegister(json["output"].get<std::string>());
		if (json.contains("dest"))
			stage.Output = ParseTevRegister(json["dest"].get<std::string>());
	}

	static void ParseTevOrders(const nlohmann::json& orders, std::vector<GXTevStage>& stages)
	{
		if (!orders.is_array())
			return;

		for (const auto& orderJson : orders)
		{
			if (!orderJson.contains("stage"))
				continue;

			const int stageIndex = orderJson["stage"].get<int>();

			if (stageIndex < 0)
				continue;

			if (stages.size() <= static_cast<size_t>(stageIndex))
				stages.resize(stageIndex + 1);

			GXTevOrder& order = stages[stageIndex].Order;

			if (orderJson.contains("tex_coord") && !orderJson["tex_coord"].is_null())
				order.TexCoord = orderJson["tex_coord"].get<int>();

			if (orderJson.contains("tex_map") && !orderJson["tex_map"].is_null())
				order.TexMap = orderJson["tex_map"].get<int>();

			if (orderJson.contains("color_channel") && !orderJson["color_channel"].is_null())
				order.ColorChannel = orderJson["color_channel"].get<int>();
		}
	}

	static void ParseTevStages(const nlohmann::json& tevStagesJson, std::vector<GXTevStage>& stages)
	{
		if (!tevStagesJson.is_array())
			return;

		for (const auto& stageJson : tevStagesJson)
		{
			int stageIndex = static_cast<int>(stages.size());

			if (stageJson.contains("stage"))
				stageIndex = stageJson["stage"].get<int>();
			else if (stageJson.contains("index"))
				stageIndex = stageJson["index"].get<int>();

			if (stageIndex < 0)
				continue;

			if (stages.size() <= static_cast<size_t>(stageIndex))
				stages.resize(stageIndex + 1);

			GXTevStage& stage = stages[stageIndex];

			if (stageJson.contains("color"))
				ParseTevColorStage(stageJson["color"], stage.ColorStage);

			if (stageJson.contains("alpha"))
				ParseTevAlphaStage(stageJson["alpha"], stage.AlphaStage);

			if (stageJson.contains("konst_color_selector"))
				stage.KonstColorSelector = stageJson["konst_color_selector"].get<int>();

			if (stageJson.contains("konst_alpha_selector"))
				stage.KonstAlphaSelector = stageJson["konst_alpha_selector"].get<int>();
		}
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

				material.TevStages.clear();

				if (okmatMaterial.contains("j3d"))
				{
					const auto& j3d = okmatMaterial["j3d"];

					if (j3d.contains("tev_orders"))
						ParseTevOrders(j3d["tev_orders"], material.TevStages);

					if (j3d.contains("tev_stages"))
						ParseTevStages(j3d["tev_stages"], material.TevStages);
				}

				if (okmatMaterial.contains("tev_orders"))
					ParseTevOrders(okmatMaterial["tev_orders"], material.TevStages);

				if (okmatMaterial.contains("tev_stages"))
					ParseTevStages(okmatMaterial["tev_stages"], material.TevStages);

				if (okmatMaterial.contains("tev_colors") && okmatMaterial["tev_colors"].is_array())
				{
					const auto& colors = okmatMaterial["tev_colors"];

					if (colors.size() > 0) material.TevColor0 = ParseVec4(colors[0]);
					if (colors.size() > 1) material.TevColor1 = ParseVec4(colors[1]);
					if (colors.size() > 2) material.TevColor2 = ParseVec4(colors[2]);
				}

				if (okmatMaterial.contains("konst_colors") && okmatMaterial["konst_colors"].is_array())
				{
					const auto& colors = okmatMaterial["konst_colors"];

					if (colors.size() > 0) material.KonstColor0 = ParseVec4(colors[0]);
					if (colors.size() > 1) material.KonstColor1 = ParseVec4(colors[1]);
					if (colors.size() > 2) material.KonstColor2 = ParseVec4(colors[2]);
					if (colors.size() > 3) material.KonstColor3 = ParseVec4(colors[3]);
				}

				material.DiffuseTexturePath = originalDiffuseTexturePath;

				std::cout << "[OKMATLoader] Applied material: " << material.Name
					<< " | alpha=" << static_cast<int>(material.Alpha)
					<< " | blend=" << material.BlendEnabled
					<< " | queue=" << material.RenderQueue
					<< " | textures=" << material.TextureSlots.size()
					<< " | tevStages=" << material.TevStages.size()
					<< std::endl;

				break;
			}
		}

		return true;
	}
}