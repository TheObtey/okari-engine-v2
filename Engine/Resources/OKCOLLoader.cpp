#include "OKCOLLoader.h"

#include <json.hpp>
#include <fstream>
#include <iostream>

namespace Okari
{
	static glm::vec3 ParseVec3(const nlohmann::json& value)
	{
		if (!value.is_array() || value.size() < 3)
			return glm::vec3(0.0f);

		return glm::vec3(
			value[0].get<float>(),
			value[1].get<float>(),
			value[2].get<float>()
		);
	}

	CollisionMesh OKCOLLoader::Load(const std::string& path)
	{
		CollisionMesh mesh;

		std::ifstream file(path);

		if (!file.is_open())
		{
			std::cerr << "[OKCOLLoader] Failed to open file: " << path << std::endl;
			return mesh;
		}

		nlohmann::json data;

		try
		{
			file >> data;
		}
		catch (const std::exception& e)
		{
			std::cerr << "[OKCOLLoader] Failed to parse JSON: " << path << " | " << e.what() << std::endl;
			return mesh;
		}

		if (!data.contains("format") || data["format"].get<std::string>() != "okcol")
		{
			std::cerr << "[OKCOLLoader] Invalid OKCOL format: " << path << std::endl;
			return mesh;
		}

		if (data.contains("source") && data["source"].contains("file"))
			mesh.SourceFile = data["source"]["file"].get<std::string>();

		if (!data.contains("collision") || !data["collision"].contains("triangles"))
		{
			std::cerr << "[OKCOLLoader] Missing collision.triangles: " << path << std::endl;
			return mesh;
		}

		const auto& triangles = data["collision"]["triangles"];

		for (const auto& triJson : triangles)
		{
			if (triJson.contains("error"))
				continue;

			if (!triJson.contains("v0") || !triJson.contains("v1") || !triJson.contains("v2"))
				continue;

			CollisionTriangle triangle;

			triangle.Index = triJson.value("index", 0);
			triangle.Material = triJson.value("material", "default");
			triangle.MaterialType = triJson.value("material_type", 0);
			triangle.AttributeRaw = triJson.value("attribute_raw", "0x0000");
			triangle.Passthrough = triJson.value("passthrough", false);

			triangle.V0 = ParseVec3(triJson["v0"]);
			triangle.V1 = ParseVec3(triJson["v1"]);
			triangle.V2 = ParseVec3(triJson["v2"]);

			if (triJson.contains("normal"))
				triangle.Normal = ParseVec3(triJson["normal"]);

			if (triJson.contains("flags") && triJson["flags"].is_array())
			{
				for (const auto& flag : triJson["flags"])
					triangle.Flags.push_back(flag.get<std::string>());
			}

			mesh.Triangles.push_back(triangle);
		}

		std::cout << "[OKCOLLoader] Loaded " << mesh.Triangles.size()
			<< " collision triangles from " << path << std::endl;

		return mesh;
	}
}