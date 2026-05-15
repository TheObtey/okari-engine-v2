#pragma once

#include "Physics/PhysicsComponent.h"
#include "Collision/CollisionWorld.h"
#include "Scene/Transform.h"

namespace Okari
{
    // Result of a ground-detection raycast.
    struct GroundHit
    {
        bool Hit = false;
        glm::vec3 Point = glm::vec3(0.0f);
        glm::vec3 Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        float Distance = 0.0f;
    };

    class PhysicsSystem
    {
    public:
        // Gravity constant matching Twilight Princess's snappy feel (m/s^2).
        static constexpr float Gravity = -20.0f;

        // Apply gravity and ground resolution to a physics component.
        // position is read and written in place.
        static void Update(
            PhysicsComponent& physics,
            glm::vec3& position,
            const CollisionWorld& collisionWorld,
            float deltaTime
        );

        // Cast a ray downward from origin and return the closest ground hit.
        static GroundHit RaycastDown(
            const glm::vec3& origin,
            float maxDistance,
            const CollisionWorld& collisionWorld
        );

        // Cast a ray in an arbitrary direction and return the closest hit.
        static GroundHit Raycast(
            const glm::vec3& origin,
            const glm::vec3& direction,
            float maxDistance,
            const CollisionWorld& collisionWorld
        );

    private:
        static bool RayIntersectsTriangle(
            const glm::vec3& origin,
            const glm::vec3& direction,
            const glm::vec3& v0,
            const glm::vec3& v1,
            const glm::vec3& v2,
            float& outT
        );
    };
}
