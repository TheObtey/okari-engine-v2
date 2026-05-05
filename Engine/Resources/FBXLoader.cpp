#include "FBXLoader.h"
#include "OKMATLoader.h"

#include "ufbx.h"

#include <vector>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <stb_image.h>

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

		if (mesh->vertex_normal.exists)
		{
			ufbx_vec3 normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, vertexIndex);

			vertex.Normal[0] = (float)normal.x;
			vertex.Normal[1] = (float)normal.y;
			vertex.Normal[2] = (float)normal.z;
		}
		else
		{
			vertex.Normal[0] = 0.0f;
			vertex.Normal[1] = 1.0f;
			vertex.Normal[2] = 0.0f;
		}

		return vertex;
	}

	static std::string ResolveTexturePath(const std::string& fbxPath, const char* texturePath)
	{
		if (!texturePath || texturePath[0] == '\0')
			return "";

		std::filesystem::path original(texturePath);
		std::string filename = original.filename().string();

		std::filesystem::path fbxDir = std::filesystem::path(fbxPath).parent_path();
		std::filesystem::path finalPath = fbxDir / filename;

		if (!std::filesystem::exists(finalPath))
		{
			std::cout << "[FBXLoad] Texture not found locally, fallback to original path: " << texturePath << std::endl;
			return texturePath;
		}

		return finalPath.generic_string();
	}

	static bool TextureHasAlphaPixels(const std::string& texturePath)
	{
		if (texturePath.empty())
			return false;

		int width = 0;
		int height = 0;
		int channels = 0;

		unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

		if (!data)
			return false;

		bool hasAlpha = false;
		int pixelCount = width * height;

		for (int i = 0; i < pixelCount; i++)
		{
			unsigned char alpha = data[i * 4 + 3];

			if (alpha < 250)
			{
				hasAlpha = true;
				break;
			}
		}

		stbi_image_free(data);
		return hasAlpha;
	}

	static Material ExtractMaterial(const std::string& fbxPath, const ufbx_material* fbxMaterial)
	{
		Material material;

		if (!fbxMaterial)
		{
			material.Name = "NullMaterial";
			return material;
		}

		material.Name = fbxMaterial->name.data ? fbxMaterial->name.data : "UnnamedMaterial";

		const ufbx_texture* diffuseTexture = nullptr;

		if (fbxMaterial->fbx.diffuse_color.texture)
			diffuseTexture = fbxMaterial->fbx.diffuse_color.texture;
		else if (fbxMaterial->pbr.base_color.texture)
			diffuseTexture = fbxMaterial->pbr.base_color.texture;

		if (diffuseTexture)
		{
			if (diffuseTexture->relative_filename.data && diffuseTexture->relative_filename.length > 0)
				material.DiffuseTexturePath = ResolveTexturePath(fbxPath, diffuseTexture->relative_filename.data);
			else if (diffuseTexture->filename.data && diffuseTexture->filename.length > 0)
				material.DiffuseTexturePath = ResolveTexturePath(fbxPath, diffuseTexture->filename.data);
		}

		material.UseAlphaCutout = TextureHasAlphaPixels(material.DiffuseTexturePath);

		std::cout << "[FBXLoader] Material: " << material.Name
			<< " | Texture: " << material.DiffuseTexturePath << std::endl;

		return material;
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
		std::vector<SubMesh> subMeshes;
		std::vector<Material> materials;

		for (size_t meshIndex = 0; meshIndex < scene->meshes.count; meshIndex++)
		{
			ufbx_mesh* mesh = scene->meshes.data[meshIndex];

			uint32_t materialBaseIndex = static_cast<uint32_t>(materials.size());

			for (size_t materialIndex = 0; materialIndex < mesh->materials.count; materialIndex++)
			{
				ufbx_material* fbxMaterial = mesh->materials.data[materialIndex];
				materials.push_back(ExtractMaterial(path, fbxMaterial));
			}

			if (mesh->materials.count == 0)
			{
				Material defaultMaterial;
				defaultMaterial.Name = "Default";
				materials.push_back(defaultMaterial);
			}

			std::vector<uint32_t> triIndices;
			triIndices.resize(mesh->max_face_triangles * 3);

			for (size_t partIndex = 0; partIndex < mesh->material_parts.count; partIndex++)
			{
				ufbx_mesh_part* part = &mesh->material_parts.data[partIndex];

				if (part->num_triangles == 0)
					continue;

				SubMesh subMesh;
				subMesh.VertexOffset = static_cast<uint32_t>(vertices.size());

				if (part->index < mesh->materials.count)
					subMesh.MaterialIndex = materialBaseIndex + static_cast<uint32_t>(part->index);
				else
					subMesh.MaterialIndex = materialBaseIndex;

				for (size_t faceListIndex = 0; faceListIndex < part->num_faces; faceListIndex++)
				{
					ufbx_face face = mesh->faces.data[part->face_indices.data[faceListIndex]];

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

				subMesh.VertexCount = static_cast<uint32_t>(vertices.size()) - subMesh.VertexOffset;

				if (subMesh.VertexCount > 0)
					subMeshes.push_back(subMesh);
			}
		}

		if (vertices.empty())
		{
			std::cerr << "[FBXLoader] No vertices found in FBX: " << path << std::endl;
			ufbx_free_scene(scene);
			return nullptr;
		}

		std::cout << "[FBXLoader] Loaded " << path
			<< " | Vertices: " << vertices.size()
			<< " | SubMeshes: " << subMeshes.size()
			<< " | Materials: " << materials.size()
			<< std::endl;

		Mesh* mesh = new Mesh(vertices, subMeshes, materials);

		ufbx_free_scene(scene);

		{
			std::filesystem::path fbxPath(path);
			std::filesystem::path okmatPath = fbxPath;
			
			okmatPath.replace_extension(".okmat");

			if (std::filesystem::exists(okmatPath))
				OKMATLoader::ApplyToMesh(mesh, okmatPath.string());

		}

		return mesh;
	}
}