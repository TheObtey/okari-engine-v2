#pragma once

#include "Rendering/Renderer.h"
#include "Rendering/Camera.h"
#include "Rendering/DirectionalLight.h"
#include "Collision/CollisionWorld.h"
#include "Scene/WorldObject.h"
#include "Actor/Actor.h"

#include <string>
#include <vector>

namespace Okari
{
	class World
	{
	public:
		~World();

		glm::mat4 GetWorldMatrix(uint64_t objectID) const;
		
		void AddObject(const WorldObject& object);

		std::vector<WorldObject>& GetObjects();
		const std::vector<WorldObject>& GetObjects() const;

		std::string GenerateUniqueName(const std::string& baseName) const;

		WorldObject& CreateObject(const std::string& name);
		bool RemoveObject(uint64_t id);
		WorldObject* DuplicateObject(uint64_t id);
		WorldObject* GetObjectByID(uint64_t id);

		bool MoveObjectBefore(uint64_t movingID, uint64_t targetID);
		bool MoveObjectToEndOfParent(uint64_t movingID, uint64_t parentID);

		bool SetParent(uint64_t childID, uint64_t parentID);
		std::vector<WorldObject*> GetChildren(uint64_t parentID);

		bool LoadFromFile(const std::string& path, std::string* outSceneName = nullptr);
		bool SaveToFile(const std::string& path, const std::string& sceneName) const;

		void Update(float deltaTime);
		void Render(Renderer& renderer, const Camera& camera, uint64_t selectedObjectID = 0);
		void RenderPicking(Renderer& renderer, const Camera& camera);

		void BuildRuntimeActors();
		void DestroyRuntimeActors();
		void UpdateActors(float deltaTime, Camera& camera);

		void EnterPlayMode();
		void ExitPlayMode();

		CollisionWorld& GetCollisionWorld() { return m_CollisionWorld; }

		bool IsPlaying() const { return m_IsPlaying; }

		Actor* GetPlayer() const { return m_Player; }

		DirectionalLight GetMainDirectionalLight() const;

	private:
		std::vector<WorldObject> m_Objects;
		std::vector<std::unique_ptr<Actor>> m_RuntimeActors;
		uint64_t m_NextID = 1;

		CollisionWorld m_CollisionWorld;

		bool m_IsPlaying = false;

		Actor* m_Player = nullptr;
	};
}