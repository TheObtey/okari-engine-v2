#include "OKMATLoader.h"

#include <json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <sstream>

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
		if (value.is_array() && value.size() >= 4)
		{
			return glm::vec4(
				value[0].get<float>(),
				value[1].get<float>(),
				value[2].get<float>(),
				value[3].get<float>()
			);
		}

		if (value.contains("color"))
		{
			const auto& color = value["color"];

			if (color.contains("normalized"))
			{
				const auto& n = color["normalized"];

				return glm::vec4(
					n.value("r", 1.0f),
					n.value("g", 1.0f),
					n.value("b", 1.0f),
					n.value("a", 1.0f)
				);
			}
		}

		return glm::vec4(1.0f);
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

		if (value == "konst" || value == "konst_color" || value == "konstc")
			return GXTevColorArg::KonstColor;
		if (value == "konst_alpha" || value == "konsta")
			return GXTevColorArg::KonstAlpha;

		if (value == "cprev" || value == "prev_color" || value == "prev")
			return GXTevColorArg::PrevColor;
		if (value == "aprev" || value == "prev_alpha")
			return GXTevColorArg::PrevAlpha;

		if (value == "c0" || value == "reg0_color")
			return GXTevColorArg::Reg0Color;
		if (value == "a0" || value == "reg0_alpha")
			return GXTevColorArg::Reg0Alpha;

		if (value == "c1" || value == "reg1_color")
			return GXTevColorArg::Reg1Color;
		if (value == "a1" || value == "reg1_alpha")
			return GXTevColorArg::Reg1Alpha;

		if (value == "c2" || value == "reg2_color")
			return GXTevColorArg::Reg2Color;
		if (value == "a2" || value == "reg2_alpha")
			return GXTevColorArg::Reg2Alpha;

		return GXTevColorArg::Zero;
	}

	static GXTevColorArg ParseTevAlphaArg(const std::string& value)
	{
		if (value == "zero")
			return GXTevColorArg::Zero;

		if (value == "one")
			return GXTevColorArg::One;

		if (value == "texa" || value == "tex_alpha")
			return GXTevColorArg::TexAlpha;

		if (value == "texc" || value == "tex_color")
			return GXTevColorArg::TexColor;

		if (value == "rasa" || value == "ras_alpha")
			return GXTevColorArg::RasAlpha;

		if (value == "rasc" || value == "ras_color")
			return GXTevColorArg::RasColor;

		if (value == "konst" || value == "konsta" || value == "konst_alpha")
			return GXTevColorArg::KonstAlpha;

		if (value == "aprev" || value == "prev_alpha")
			return GXTevColorArg::PrevAlpha;

		if (value == "cprev" || value == "prev_color")
			return GXTevColorArg::PrevColor;

		if (value == "a0" || value == "reg0_alpha")
			return GXTevColorArg::Reg0Alpha;

		if (value == "c0" || value == "reg0_color")
			return GXTevColorArg::Reg0Color;

		if (value == "a1" || value == "reg1_alpha")
			return GXTevColorArg::Reg1Alpha;

		if (value == "c1" || value == "reg1_color")
			return GXTevColorArg::Reg1Color;

		if (value == "a2" || value == "reg2_alpha")
			return GXTevColorArg::Reg2Alpha;

		if (value == "c2" || value == "reg2_color")
			return GXTevColorArg::Reg2Color;

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

		if (json.contains("bias_value"))
			stage.Bias = json["bias_value"].get<int>();
		if (json.contains("scale_value"))
			stage.Scale = json["scale_value"].get<int>();
		if (json.contains("clamp_value"))
			stage.Clamp = json["clamp_value"].get<int>() != 0;

		if (json.contains("output"))
			stage.Output = ParseTevRegister(json["output"].get<std::string>());
		if (json.contains("dest"))
			stage.Output = ParseTevRegister(json["dest"].get<std::string>());
	}

	static void ParseTevAlphaStage(const nlohmann::json& json, GXTevAlphaStage& stage)
	{
		if (json.contains("a"))
			stage.A = ParseTevAlphaArg(json["a"].get<std::string>());
		if (json.contains("b"))
			stage.B = ParseTevAlphaArg(json["b"].get<std::string>());
		if (json.contains("c"))
			stage.C = ParseTevAlphaArg(json["c"].get<std::string>());
		if (json.contains("d"))
			stage.D = ParseTevAlphaArg(json["d"].get<std::string>());

		if (json.contains("op"))
			stage.Operation = ParseTevOp(json["op"].get<std::string>());
		if (json.contains("operation"))
			stage.Operation = ParseTevOp(json["operation"].get<std::string>());

		if (json.contains("bias_value"))
			stage.Bias = json["bias_value"].get<int>();
		if (json.contains("scale_value"))
			stage.Scale = json["scale_value"].get<int>();
		if (json.contains("clamp_value"))
			stage.Clamp = json["clamp_value"].get<int>() != 0;

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

	static void ParseTevBlock(const nlohmann::json& tev, Material& material)
	{
		if (!tev.is_object())
			return;

		if (tev.contains("colors") && tev["colors"].is_array())
		{
			const auto& colors = tev["colors"];

			if (colors.size() > 0) material.TevColor0 = ParseVec4(colors[0]);
			if (colors.size() > 1) material.TevColor1 = ParseVec4(colors[1]);
			if (colors.size() > 2) material.TevColor2 = ParseVec4(colors[2]);
		}

		if (tev.contains("konst_colors") && tev["konst_colors"].is_array())
		{
			const auto& colors = tev["konst_colors"];

			if (colors.size() > 0) material.KonstColor0 = ParseVec4(colors[0]);
			if (colors.size() > 1) material.KonstColor1 = ParseVec4(colors[1]);
			if (colors.size() > 2) material.KonstColor2 = ParseVec4(colors[2]);
			if (colors.size() > 3) material.KonstColor3 = ParseVec4(colors[3]);
		}

		if (tev.contains("konst_selectors"))
		{
			const auto& selectors = tev["konst_selectors"];

			if (selectors.contains("color") && selectors["color"].is_array())
			{
				const auto& colorSelectors = selectors["color"];

				for (size_t i = 0; i < material.TevStages.size() && i < colorSelectors.size(); i++)
				{
					if (!colorSelectors[i].is_null())
						material.TevStages[i].KonstColorSelector = colorSelectors[i].get<int>();
				}
			}

			if (selectors.contains("alpha") && selectors["alpha"].is_array())
			{
				const auto& alphaSelectors = selectors["alpha"];

				for (size_t i = 0; i < material.TevStages.size() && i < alphaSelectors.size(); i++)
				{
					if (!alphaSelectors[i].is_null())
						material.TevStages[i].KonstAlphaSelector = alphaSelectors[i].get<int>();
				}
			}
		}
	}

	static const char* TevArgToString(GXTevColorArg arg)
	{
		switch (arg)
		{
		case GXTevColorArg::Zero: return "Zero";
		case GXTevColorArg::One: return "One";
		case GXTevColorArg::TexColor: return "TexColor";
		case GXTevColorArg::TexAlpha: return "TexAlpha";
		case GXTevColorArg::RasColor: return "RasColor";
		case GXTevColorArg::RasAlpha: return "RasAlpha";
		case GXTevColorArg::KonstColor: return "KonstColor";
		case GXTevColorArg::KonstAlpha: return "KonstAlpha";
		case GXTevColorArg::PrevColor: return "PrevColor";
		case GXTevColorArg::PrevAlpha: return "PrevAlpha";
		case GXTevColorArg::Reg0Color: return "Reg0Color";
		case GXTevColorArg::Reg0Alpha: return "Reg0Alpha";
		case GXTevColorArg::Reg1Color: return "Reg1Color";
		case GXTevColorArg::Reg1Alpha: return "Reg1Alpha";
		case GXTevColorArg::Reg2Color: return "Reg2Color";
		case GXTevColorArg::Reg2Alpha: return "Reg2Alpha";
		default: return "Unknown";
		}
	}

	static const char* TevRegisterToString(GXTevRegister reg)
	{
		switch (reg)
		{
		case GXTevRegister::Prev: return "Prev";
		case GXTevRegister::Reg0: return "Reg0";
		case GXTevRegister::Reg1: return "Reg1";
		case GXTevRegister::Reg2: return "Reg2";
		default: return "Unknown";
		}
	}

	static void DumpTevMaterial(const Material& material)
	{
		std::cout << "[TEV Dump] Material: " << material.Name << std::endl;

		auto PrintVec4 = [](const char* name, const glm::vec4& v)
			{
				std::cout << "  " << name << " = "
					<< v.r << ", "
					<< v.g << ", "
					<< v.b << ", "
					<< v.a
					<< std::endl;
			};

		PrintVec4("TevColor0", material.TevColor0);
		PrintVec4("TevColor1", material.TevColor1);
		PrintVec4("TevColor2", material.TevColor2);

		PrintVec4("KonstColor0", material.KonstColor0);
		PrintVec4("KonstColor1", material.KonstColor1);
		PrintVec4("KonstColor2", material.KonstColor2);
		PrintVec4("KonstColor3", material.KonstColor3);

		for (size_t i = 0; i < material.TevStages.size(); i++)
		{
			const GXTevStage& stage = material.TevStages[i];

			std::cout << "  Stage " << i
				<< " | texMap=" << stage.Order.TexMap
				<< " | texCoord=" << stage.Order.TexCoord
				<< " | colorChannel=" << stage.Order.ColorChannel
				<< " | kcSel=" << stage.KonstColorSelector
				<< " | kaSel=" << stage.KonstAlphaSelector
				<< std::endl;

			std::cout << "    Color: "
				<< TevArgToString(stage.ColorStage.A) << ", "
				<< TevArgToString(stage.ColorStage.B) << ", "
				<< TevArgToString(stage.ColorStage.C) << ", "
				<< TevArgToString(stage.ColorStage.D)
				<< " -> " << TevRegisterToString(stage.ColorStage.Output)
				<< std::endl;

			std::cout << "    Alpha: "
				<< TevArgToString(stage.AlphaStage.A) << ", "
				<< TevArgToString(stage.AlphaStage.B) << ", "
				<< TevArgToString(stage.AlphaStage.C) << ", "
				<< TevArgToString(stage.AlphaStage.D)
				<< " -> " << TevRegisterToString(stage.AlphaStage.Output)
				<< std::endl;
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

		std::string fileContent(
			(std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>()
		);

		const std::string materialsKey = "\"materials\"";
		const size_t materialsPos = fileContent.find(materialsKey);

		if (materialsPos == std::string::npos)
		{
			std::cerr << "[OKMATLoader] Invalid OKMAT, missing materials array: " << okmatPath << std::endl;
			return false;
		}

		const size_t arrayStart = fileContent.find('[', materialsPos + materialsKey.size());

		if (arrayStart == std::string::npos)
		{
			std::cerr << "[OKMATLoader] Invalid OKMAT, malformed materials array: " << okmatPath << std::endl;
			return false;
		}

		int depth = 0;
		size_t arrayEnd = arrayStart;

		for (size_t i = arrayStart; i < fileContent.size(); ++i)
		{
			if (fileContent[i] == '[')
				++depth;
			else if (fileContent[i] == ']')
			{
				--depth;
				if (depth == 0)
				{
					arrayEnd = i;
					break;
				}
			}
		}

		std::string materialsJson = "{\"materials\":" + fileContent.substr(arrayStart, arrayEnd - arrayStart + 1) + "}";

		nlohmann::json data;

		try
		{
			data = nlohmann::json::parse(materialsJson);
		}
		catch (const nlohmann::json::parse_error& e)
		{
			std::cerr << "[OKMATLoader] JSON parse error in materials section of: " << okmatPath << "\n"
				<< "  " << e.what() << std::endl;
			return false;
		}

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

					if (okmatMaterial.contains("tev"))
						ParseTevBlock(okmatMaterial["tev"], material);
				}

				if (okmatMaterial.contains("tev_orders"))
					ParseTevOrders(okmatMaterial["tev_orders"], material.TevStages);

				if (okmatMaterial.contains("tev_stages"))
					ParseTevStages(okmatMaterial["tev_stages"], material.TevStages);

				material.DiffuseTexturePath = originalDiffuseTexturePath;

				//std::cout << "[OKMATLoader] Applied material: " << material.Name
				//	<< " | alpha=" << static_cast<int>(material.Alpha)
				//	<< " | blend=" << material.BlendEnabled
				//	<< " | queue=" << material.RenderQueue
				//	<< " | textures=" << material.TextureSlots.size()
				//	<< " | tevStages=" << material.TevStages.size()
				//	<< std::endl;

				//DumpTevMaterial(material);

				break;
			}
		}

		return true;
	}
}