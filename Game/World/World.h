#pragma once

#include "Scene/Transform.h"
#include "Scene/WorldObject.h"
#include "Rendering/Renderer.h"
#include "Rendering/Camera.h"
#include <string>
#include <vector>

namespace Okari
{
	class World
	{
	public:
		void AddObject(const WorldObject& object);
		std::vector<WorldObject>& GetObjects();
		const std::vector<WorldObject>& GetObjects() const;

		std::string GenerateUniqueName(const std::string& baseName) const;

		WorldObject& CreateObject(const std::string& name);
		bool RemoveObject(uint64_t id);
		WorldObject* DuplicateObject(uint64_t id);
		WorldObject* GetObjectByID(uint64_t id);

		bool LoadFromFile(const std::string& path);
		bool SaveToFile(const std::string& path) const;

		void Update(float deltaTime);
		void Render(Renderer& renderer, const Camera& camera);

	private:
		std::vector<WorldObject> m_Objects;
		uint64_t m_NextID = 1;
	};
}