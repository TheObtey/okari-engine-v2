#include "World.h"
#include "Actors/ActorFactory.h"
#include "Actors/PlayerActor.h"
#include "Resources/MeshManager.h"

#include "json.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

namespace Okari
{
    World::~World()
    {
        DestroyRuntimeActors();
    }

    static glm::vec3 ReadVec3(const json& value)
    {
        return glm::vec3(
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>()
        );
    }

    static json WriteVec3(const glm::vec3& value)
    {
        return json::array({ value.x, value.y, value.z });
    }

    void World::AddObject(const WorldObject& object)
    {
        m_Objects.push_back(object);
    }

    std::vector<WorldObject>& World::GetObjects()
    {
        return m_Objects;
    }

    const std::vector<WorldObject>& World::GetObjects() const
    {
        return m_Objects;
    }

    std::string World::GenerateUniqueName(const std::string& baseName) const
    {
        auto nameExists = [&](const std::string& name)
            {
                for (const auto& obj : m_Objects)
                {
                    if (obj.Name == name)
                        return true;
                }

                return false;
            };

        if (!nameExists(baseName))
            return baseName;

        constexpr int MAX_ATTEMPTS = 100000;

        for (int i = 1; i <= MAX_ATTEMPTS; i++)
        {
            std::string candidate = baseName + " (" + std::to_string(i) + ")";

            if (!nameExists(candidate))
                return candidate;
        }

        return baseName + " (UniqueNameFailed)";
    }

    WorldObject& World::CreateObject(const std::string& name)
    {
        WorldObject obj;
        obj.ID = m_NextID++;
        obj.Name = GenerateUniqueName(name);

        if (!obj.Mesh.MeshPath.empty())
            obj.Mesh.RuntimeMesh = MeshManager::Get().LoadMesh(obj.Mesh.MeshPath);

        m_Objects.push_back(obj);
        return m_Objects.back();
    }

    bool World::RemoveObject(uint64_t id)
    {
        if (id == 0)
            return false;

        bool removedSomething = false;

        std::vector<uint64_t> childrenToRemove;

        for (const auto& obj : m_Objects)
        {
            if (obj.ParentID == id)
                childrenToRemove.push_back(obj.ID);
        }

        for (uint64_t childID : childrenToRemove)
            RemoveObject(childID);

        auto it = std::find_if(m_Objects.begin(), m_Objects.end(),
            [id](const WorldObject& obj)
            {
                return obj.ID == id;
            }
        );

        if (it != m_Objects.end())
        {
            m_Objects.erase(it);
            removedSomething = true;
        }

        return removedSomething;
    }

    WorldObject* World::DuplicateObject(uint64_t id)
    {
        WorldObject* original = GetObjectByID(id);
        if (!original)
            return nullptr;
        
        WorldObject copy = *original;
        copy.ID = m_NextID++;
        copy.Name = GenerateUniqueName(copy.Name);

        m_Objects.push_back(copy);
        return &m_Objects.back();
    }

    WorldObject* World::GetObjectByID(uint64_t id)
    {
        for (auto& obj : m_Objects)
        {
            if (obj.ID == id)
                return &obj;
        }

        return nullptr;
    }

    bool World::MoveObjectBefore(uint64_t movingID, uint64_t targetID)
    {
        if (movingID == targetID)
            return false;
        
        auto movingIt = std::find_if(m_Objects.begin(), m_Objects.end(),
            [movingID](const WorldObject& obj) { return obj.ID == movingID; });

        auto targetIt = std::find_if(m_Objects.begin(), m_Objects.end(),
            [targetID](const WorldObject& obj) { return obj.ID == targetID; });

        if (movingIt == m_Objects.end() || targetIt == m_Objects.end())
            return false;

        WorldObject movingObject = *movingIt;
        m_Objects.erase(movingIt);

        targetIt = std::find_if(m_Objects.begin(), m_Objects.end(),
            [targetID](const WorldObject& obj) { return obj.ID == targetID; });

        m_Objects.insert(targetIt, movingObject);

        return true;
    }

    bool World::MoveObjectToEndOfParent(uint64_t movingID, uint64_t parentID)
    {
        WorldObject* movingObj = GetObjectByID(movingID);
        if (!movingObj)
            return false;

        if (!SetParent(movingID, parentID))
            return false;

        auto movingIt = std::find_if(m_Objects.begin(), m_Objects.end(),
            [movingID](const WorldObject& obj) { return obj.ID == movingID; });
        
        if (movingIt == m_Objects.end())
            return false;

        WorldObject movingCopy = *movingIt;
        m_Objects.erase(movingIt);
        m_Objects.push_back(movingCopy);

        return true;
    }

    bool World::SetParent(uint64_t childID, uint64_t parentID)
    {
        if (childID == parentID)
            return false;

        WorldObject* child = GetObjectByID(childID);
        if (!child)
            return false;

        uint64_t current = parentID;

        while (current != 0)
        {
            if (current == childID)
                return false;

            WorldObject* obj = GetObjectByID(current);
            if (!obj)
                break;

            current = obj->ParentID;
        }

        child->ParentID = parentID;
        
        return true;
    }

    std::vector<WorldObject*> World::GetChildren(uint64_t parentID)
    {
        std::vector<WorldObject*> result;

        for (auto& obj : m_Objects)
        {
            if (obj.ParentID == parentID)
                result.push_back(&obj);
        }

        return result;
    }

    DirectionalLight World::GetMainDirectionalLight() const
    {
        DirectionalLight light;

        for (const WorldObject& obj : m_Objects)
        {
            if (!obj.Enabled)
                continue;

            if (obj.ActorType != "DirectionalLight")
                continue;

            if (obj.ActorData.contains("direction"))
                light.Direction = ReadVec3(obj.ActorData["direction"]);

            if (obj.ActorData.contains("color"))
                light.Color = ReadVec3(obj.ActorData["color"]);
            
            if (obj.ActorData.contains("intensity"))
                light.Intensity = obj.ActorData["intensity"].get<float>();
            
            if (obj.ActorData.contains("ambiant"))
                light.Ambiant = ReadVec3(obj.ActorData["ambiant"]);

            return light;
        }

        return light;
    }

    bool World::LoadFromFile(const std::string& path, std::string* outSceneName)
    {
        DestroyRuntimeActors();
        
        m_IsPlaying = false;

        std::ifstream file(path);

        if (!file.is_open())
        {
            std::cerr << "Failed to open level file: " << path << std::endl;
            return false;
        }

        json data;
        file >> data;

        if (data.contains("fileType") && data["fileType"] != "Okari.Scene")
        {
            std::cerr << "Selected file is not a scene: " << path << std::endl;
            return false;
        }

        if (outSceneName)
        {
            if (data.contains("name"))
                *outSceneName = data["name"].get<std::string>();
            else
                *outSceneName = "untitled";
        }

        m_Objects.clear();
        m_NextID = 1;

        for (const auto& obj : data["objects"])
        {
            WorldObject worldObject;

            if (obj.contains("enabled"))
                worldObject.Enabled = obj["enabled"];

            if (obj.contains("actorType"))
                worldObject.ActorType = obj["actorType"];

            if (obj.contains("id"))
                worldObject.ID = obj["id"].get<uint64_t>();
            else
                worldObject.ID = m_NextID++;

            m_NextID = std::max(m_NextID, worldObject.ID + 1);

            if (obj.contains("parentId"))
                worldObject.ParentID = obj["parentId"].get<uint64_t>();
            else
                worldObject.ParentID = 0;

            worldObject.Name = obj["name"].get<std::string>();
            worldObject.Transform.Position = ReadVec3(obj["position"]);
            worldObject.Transform.Rotation = ReadVec3(obj["rotation"]);
            worldObject.Transform.Scale = ReadVec3(obj["scale"]);
            
            if (obj.contains("meshComponent"))
            {
                const auto& mesh = obj["meshComponent"];

                if (mesh.contains("enabled"))
                    worldObject.Mesh.Enabled = mesh["enabled"].get<bool>();

                if (mesh.contains("mesh"))
                    worldObject.Mesh.MeshPath = mesh["mesh"].get<std::string>();

                if (mesh.contains("texture"))
                    worldObject.Mesh.TexturePath = mesh["texture"].get<std::string>();
            }
            else if (obj.contains("texture"))
                worldObject.Mesh.TexturePath = obj["texture"].get<std::string>();

            if (obj.contains("actorData"))
                worldObject.ActorData = obj["actorData"];

            m_Objects.push_back(worldObject);
        }

        std::cout << "Loaded level: " << path << std::endl;

        return true;
    }

    bool World::SaveToFile(const std::string& path, const std::string& sceneName) const
    {
        json data;
        data["name"] = sceneName.c_str();
        data["objects"] = json::array();

        for (const auto& obj : m_Objects)
        {
            json jsonObj;
            jsonObj["fileType"] = "Okari.Scene";
            jsonObj["enabled"] = obj.Enabled;
            jsonObj["actorType"] = obj.ActorType;
            jsonObj["id"] = obj.ID;
            jsonObj["parentId"] = obj.ParentID;
            jsonObj["name"] = obj.Name;
            jsonObj["type"] = "Cube";
            jsonObj["meshComponent"] = {
                { "enabled", obj.Mesh.Enabled },
                { "mesh", obj.Mesh.MeshPath },
                { "texture", obj.Mesh.TexturePath }
            };
            jsonObj["position"] = WriteVec3(obj.Transform.Position);
            jsonObj["rotation"] = WriteVec3(obj.Transform.Rotation);
            jsonObj["scale"] = WriteVec3(obj.Transform.Scale);
            jsonObj["actorData"] = obj.ActorData;

            data["objects"].push_back(jsonObj);
        }

        std::ofstream file(path);

        if (!file.is_open())
        {
            std::cerr << "Failed to save level file: " << path << std::endl;
            return false;
        }

        file << data.dump(4);

        std::cout << "Saved level: " << path << std::endl;

        return true;
    }

    void World::Update(float)
    { }

    void World::Render(Renderer& renderer, const Camera& camera, uint64_t selectedObjectID)
    {
        DirectionalLight light = GetMainDirectionalLight();

        for (auto& obj : m_Objects)
            if (obj.Mesh.Enabled)
            {
                if (!obj.Mesh.RuntimeMesh && !obj.Mesh.MeshPath.empty())
                    obj.Mesh.RuntimeMesh = MeshManager::Get().LoadMesh(obj.Mesh.MeshPath);

                if (obj.Mesh.RuntimeMesh)
                    renderer.DrawMesh(obj.Transform, obj.Mesh.RuntimeMesh, obj.Mesh.TexturePath, camera, light);
                else
                    renderer.DrawCube(obj.Transform, obj.Mesh.TexturePath, camera);
            }

        if (selectedObjectID != 0)
        {
            if (WorldObject* selectedObject = GetObjectByID(selectedObjectID))
                if (selectedObject->Mesh.RuntimeMesh)
                    renderer.DrawMeshOutline(selectedObject->Transform, selectedObject->Mesh.RuntimeMesh, camera);
                else
                    renderer.DrawCubeOutline(selectedObject->Transform, camera);
        }

        if (m_IsPlaying && m_Player)
        {
            std::string playerTexture = "Assets/Textures/link.png";

            if (auto* player = dynamic_cast<PlayerActor*>(m_Player))
            {
                if (player->GetMesh())
                {
                    playerTexture = player->GetTexturePath();

                    renderer.DrawMesh(
                        player->GetTransform(),
                        player->GetMesh(),
                        playerTexture,
                        camera,
                        light
                    );
                }
            }
        }
    }

    void World::RenderPicking(Renderer& renderer, const Camera& camera)
    {
        for (auto& obj : m_Objects)
        {
            renderer.DrawCubeID(
                obj.Transform,
                static_cast<uint32_t>(obj.ID),
                camera
            );
        }
    }

    void World::BuildRuntimeActors()
    {
        DestroyRuntimeActors();

        for (WorldObject& object : m_Objects)
        {
            if (!object.Enabled)
                continue;

            if (object.ActorType.empty() || object.ActorType == "None")
                continue;

            std::unique_ptr<Actor> actor = ActorFactory::CreateActor(object.ActorType, &object);

            if (!actor)
                continue;

            actor->OnCreate();
            m_RuntimeActors.push_back(std::move(actor));

        }

        std::unique_ptr<Actor> player = ActorFactory::CreateActor("Player", nullptr);

        if (player)
        {
            player->OnCreate();
            m_Player = player.get();
            m_RuntimeActors.push_back(std::move(player));
        }
    }

    void World::DestroyRuntimeActors()
    {
        for (auto& actor : m_RuntimeActors)
        {
            if (actor)
                actor->OnDestroy();
        }

        m_RuntimeActors.clear();
    }

    void World::UpdateActors(float deltaTime, Camera& camera)
    {
        if (!m_IsPlaying)
            return;

        for (auto& actor : m_RuntimeActors)
        {
            if (actor)
                actor->OnUpdate(deltaTime);
        }

        if (auto* player = dynamic_cast<PlayerActor*>(m_Player))
        {
            player->UpdateMovement(deltaTime, camera);
            
            const glm::vec3 playerPos = player->GetTransform().Position;
            const glm::vec3 cameraOffset = glm::vec3(0.0f, 2.5f, 6.0f);

            camera.SetPosition(playerPos + cameraOffset);
            camera.SetTarget(playerPos);
        }
    }

    void World::EnterPlayMode()
    {
        if (m_IsPlaying)
            return;

        m_IsPlaying = true;
        BuildRuntimeActors();
    }

    void World::ExitPlayMode()
    {
        if (!m_IsPlaying)
            return;

        m_IsPlaying = false;
        DestroyRuntimeActors();
    }
}