#pragma once

#include "Rendering/Mesh.h"

#include <unordered_map>
#include <string>

namespace Okari
{
	class MeshManager
	{
	public:
		static MeshManager& Get();

		Mesh* LoadMesh(const std::string& path);

		void Clear();

	private:
		std::unordered_map<std::string, Mesh*> m_MeshCache;

		Mesh* CreateDebugMesh();
	};
}