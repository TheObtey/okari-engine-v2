#include "Resources/FBXLoader.h"

#include "ufbx.h"
#include <vector>
#include <iostream>

namespace Okari
{
	static Vertex MakeVertex(const ufbx_mesh* mesh, uint32_t vertexIndex)
	{
		Vertex vertex{};

		ufbx_vec3 position = ufbx_get_vertex_vec3(&mesh->vertex_position, vertexIndex);

		vertex.Position[0] = (float)position.x;
		vertex.Position[1] = (float)position.y;
		vertex.Position[2] = (float)position.z;

		if (mesh->vertex_uv.exists)
		{
			ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, vertexIndex);

			vertex.UV[0] = (float)uv.x;
			vertex.UV[1] = (float)uv.y;
		}
		else
		{
			vertex.UV[0] = 0.0f;
			vertex.UV[1] = 0.0f;
		}

		return vertex;
	}

	Mesh* FBXLoader::LoadMesh(const std::string& path)
	{
		ufbx_load_opts opts = {};
		ufbx_error error;

		ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &error);

		if (!scene)
		{
			std::cerr << "[FBXLoader] Failed to load FBX: " << path << std::endl;
			std::cerr << "[FBXLoader] " << error.description.data << std::endl;
			return nullptr;
		}

		std::vector<Vertex> vertices;

		for (size_t meshIndex = 0; meshIndex < scene->meshes.count; meshIndex++)
		{
			ufbx_mesh* mesh = scene->meshes.data[meshIndex];

			std::vector<uint32_t> triIndices;
			triIndices.resize(mesh->max_face_triangles * 3);

			for (size_t faceIndex = 0; faceIndex < mesh->faces.count; faceIndex++)
			{
				ufbx_face face = mesh->faces.data[faceIndex];

				uint32_t numTriangles = ufbx_triangulate_face(
					triIndices.data(),
					triIndices.size(),
					mesh,
					face
				);

				for (uint32_t tri = 0; tri < numTriangles; tri++)
				{
					uint32_t i0 = triIndices[tri * 3 + 0];
					uint32_t i1 = triIndices[tri * 3 + 1];
					uint32_t i2 = triIndices[tri * 3 + 2];

					vertices.push_back(MakeVertex(mesh, i0));
					vertices.push_back(MakeVertex(mesh, i1));
					vertices.push_back(MakeVertex(mesh, i2));
				}
			}
		}

		if (vertices.empty())
		{
			std::cerr << "[FBXLoader] No vertices found in FBX: " << path << std::endl;
			ufbx_free_scene(scene);
			return nullptr;
		}

		std::cout << "[FBXLoader] Loaded " << path << " with " << vertices.size() << " vertices." << std::endl;

		Mesh* result = new Mesh(vertices);

		ufbx_free_scene(scene);

		return result;
	}
}