#include "MeshManager.h"
#include "FBXLoader.h"

#include <iostream>

namespace Okari
{
	MeshManager& MeshManager::Get()
	{
		static MeshManager instance;
		return instance;
	}

	Mesh* MeshManager::LoadMesh(const std::string& path)
	{
		if (path.empty())
			return nullptr;

		auto it = m_MeshCache.find(path);

		if (it != m_MeshCache.end())
			return it->second;

		Mesh* mesh = FBXLoader::LoadMesh(path);

		if (!mesh)
		{
			std::cerr << "[MeshManager] Failed to load mesh: " << path << std::endl;
			mesh = CreateDebugMesh();
		}

		m_MeshCache[path] = mesh;

		return mesh;
	}

	Mesh* MeshManager::CreateDebugMesh()
	{
		std::vector<Vertex> vertices =
		{
			{ {-0.5f,-0.5f, 0.5f}, {0,0} },
			{ { 0.5f,-0.5f, 0.5f}, {1,0} },
			{ { 0.5f, 0.5f, 0.5f}, {1,1} },
			{ {-0.5f,-0.5f, 0.5f}, {0,0} },
			{ { 0.5f, 0.5f, 0.5f}, {1,1} },
			{ {-0.5f, 0.5f, 0.5f}, {0,1} },

			{ { 0.5f,-0.5f,-0.5f}, {0,0} },
			{ {-0.5f,-0.5f,-0.5f}, {1,0} },
			{ {-0.5f, 0.5f,-0.5f}, {1,1} },
			{ { 0.5f,-0.5f,-0.5f}, {0,0} },
			{ {-0.5f, 0.5f,-0.5f}, {1,1} },
			{ { 0.5f, 0.5f,-0.5f}, {0,1} },

			{ {-0.5f,-0.5f,-0.5f}, {0,0} },
			{ {-0.5f,-0.5f, 0.5f}, {1,0} },
			{ {-0.5f, 0.5f, 0.5f}, {1,1} },
			{ {-0.5f,-0.5f,-0.5f}, {0,0} },
			{ {-0.5f, 0.5f, 0.5f}, {1,1} },
			{ {-0.5f, 0.5f,-0.5f}, {0,1} },

			{ { 0.5f,-0.5f, 0.5f}, {0,0} },
			{ { 0.5f,-0.5f,-0.5f}, {1,0} },
			{ { 0.5f, 0.5f,-0.5f}, {1,1} },
			{ { 0.5f,-0.5f, 0.5f}, {0,0} },
			{ { 0.5f, 0.5f,-0.5f}, {1,1} },
			{ { 0.5f, 0.5f, 0.5f}, {0,1} },

			{ {-0.5f, 0.5f, 0.5f}, {0,0} },
			{ { 0.5f, 0.5f, 0.5f}, {1,0} },
			{ { 0.5f, 0.5f,-0.5f}, {1,1} },
			{ {-0.5f, 0.5f, 0.5f}, {0,0} },
			{ { 0.5f, 0.5f,-0.5f}, {1,1} },
			{ {-0.5f, 0.5f,-0.5f}, {0,1} },

			{ {-0.5f,-0.5f,-0.5f}, {0,0} },
			{ { 0.5f,-0.5f,-0.5f}, {1,0} },
			{ { 0.5f,-0.5f, 0.5f}, {1,1} },
			{ {-0.5f,-0.5f,-0.5f}, {0,0} },
			{ { 0.5f,-0.5f, 0.5f}, {1,1} },
			{ {-0.5f,-0.5f, 0.5f}, {0,1} },
		};

		return new Mesh(vertices);
	}

	void MeshManager::Clear()
	{
		for (auto& [path, mesh] : m_MeshCache)
			delete mesh;

		m_MeshCache.clear();
	}
}