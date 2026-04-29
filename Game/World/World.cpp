#include "World.h"
#include "../../External/nlohmann/json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace Okari
{
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

    bool World::LoadFromFile(const std::string& path)
    {
        std::ifstream file(path);

        if (!file.is_open())
        {
            std::cerr << "Failed to open level file: " << path << std::endl;
            return false;
        }

        json data;
        file >> data;

        m_Objects.clear();

        for (const auto& obj : data["objects"])
        {
            WorldObject worldObject;
            worldObject.Name = obj["name"].get<std::string>();
            worldObject.Transform.Position = ReadVec3(obj["position"]);
            worldObject.Transform.Rotation = ReadVec3(obj["rotation"]);
            worldObject.Transform.Scale = ReadVec3(obj["scale"]);
            worldObject.TexturePath = obj["texture"].get<std::string>();

            m_Objects.push_back(worldObject);
        }

        std::cout << "Loaded level: " << path << std::endl;

        return true;
    }

    bool World::SaveToFile(const std::string& path) const
    {
        json data;
        data["name"] = "Saved Level";
        data["objects"] = json::array();

        for (const auto& obj : m_Objects)
        {
            json jsonObj;
            jsonObj["name"] = "WorldObject";
            jsonObj["type"] = "Cube";
            jsonObj["texture"] = obj.TexturePath;
            jsonObj["position"] = WriteVec3(obj.Transform.Position);
            jsonObj["rotation"] = WriteVec3(obj.Transform.Rotation);
            jsonObj["scale"] = WriteVec3(obj.Transform.Scale);

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

    void World::Render(Renderer& renderer, const Camera& camera)
    {
        for (auto& obj : m_Objects)
        {
            renderer.DrawCube(obj.Transform, obj.TexturePath, camera);
        }
    }
}