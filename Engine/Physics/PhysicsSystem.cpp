#include "PhysicsSystem.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <limits>
#include <cmath>

namespace Okari
{
    // Möller–Trumbore intersection algorithm.
    // Returns true and sets outT to the ray parameter if the ray hits the triangle.
    bool PhysicsSystem::RayIntersectsTriangle(
        const glm::vec3& origin,
        const glm::vec3& direction,
        const glm::vec3& v0,
        const glm::vec3& v1,
        const glm::vec3& v2,
        float& outT)
    {
        constexpr float EPSILON = 1e-6f;

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;

        glm::vec3 h = glm::cross(direction, edge2);
        float det = glm::dot(edge1, h);

        // Ray is parallel to the triangle.
        if (det > -EPSILON && det < EPSILON)
            return false;

        float invDet = 1.0f / det;
        glm::vec3 s = origin - v0;

        float u = glm::dot(s, h) * invDet;
        if (u < 0.0f || u > 1.0f)
            return false;

        glm::vec3 q = glm::cross(s, edge1);
        float v = glm::dot(direction, q) * invDet;
        if (v < 0.0f || u + v > 1.0f)
            return false;

        outT = glm::dot(edge2, q) * invDet;
        return outT > EPSILON;
    }

    GroundHit PhysicsSystem::Raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance,
        const CollisionWorld& collisionWorld)
    {
        GroundHit result;

        float closestT = std::numeric_limits<float>::max();

        for (const CollisionEntry& entry : collisionWorld.GetEntries())
        {
            const glm::mat4& worldMatrix = entry.WorldMatrix;

            // Bring the ray into the mesh's local space to avoid transforming
            // every triangle into world space.
            glm::mat4 invWorld = glm::inverse(worldMatrix);
            glm::vec3 localOrigin = glm::vec3(invWorld * glm::vec4(origin, 1.0f));
            glm::vec3 localDir   = glm::vec3(invWorld * glm::vec4(direction, 0.0f));

            // Renormalise after the transform (non-uniform scales affect length).
            float dirLen = glm::length(localDir);
            if (dirLen < 1e-6f)
                continue;
            glm::vec3 localDirNorm = localDir / dirLen;

            for (const CollisionTriangle& tri : entry.Mesh.Triangles)
            {
                if (tri.Passthrough)
                    continue;

                float t = 0.0f;
                if (!RayIntersectsTriangle(localOrigin, localDirNorm, tri.V0, tri.V1, tri.V2, t))
                    continue;

                // Convert t back to world-space distance.
                float worldT = t / dirLen;

                if (worldT < closestT && worldT <= maxDistance)
                {
                    closestT = worldT;

                    result.Hit = true;
                    result.Distance = worldT;
                    result.Point = origin + direction * worldT;

                    // Transform the triangle normal to world space.
                    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldMatrix)));
                    result.Normal = glm::normalize(normalMatrix * tri.Normal);
                }
            }
        }

        return result;
    }

    GroundHit PhysicsSystem::RaycastDown(
        const glm::vec3& origin,
        float maxDistance,
        const CollisionWorld& collisionWorld)
    {
        return Raycast(origin, glm::vec3(0.0f, -1.0f, 0.0f), maxDistance, collisionWorld);
    }

    void PhysicsSystem::Update(
        PhysicsComponent& physics,
        glm::vec3& position,
        const CollisionWorld& collisionWorld,
        float deltaTime)
    {
        if (!physics.UseGravity)
            return;

        // --- Ground detection ---
        // Cast from the actor's centre (feet + half-height) downward.
        // The total check distance covers the capsule half-height plus a snap margin.
        glm::vec3 rayOrigin = position + glm::vec3(0.0f, physics.CapsuleHalfHeight, 0.0f);
        float checkDistance = physics.CapsuleHalfHeight + physics.GroundSnapDistance;

        GroundHit ground = RaycastDown(rayOrigin, checkDistance, collisionWorld);

        // Validate slope — don't treat steep walls as ground.
        bool validGround = false;
        if (ground.Hit)
        {
            float slopeDeg = glm::degrees(std::acos(glm::clamp(ground.Normal.y, 0.0f, 1.0f)));
            validGround = slopeDeg <= physics.MaxSlopeAngle;
        }

        if (validGround)
        {
            physics.IsGrounded = true;

            // Snap feet to the ground surface.
            position.y = ground.Point.y;

            // Kill downward velocity; keep a tiny negative value so the next
            // frame's raycast still reaches the ground (TP-style sticky feet).
            if (physics.Velocity.y <= 0.0f)
                physics.Velocity.y = -2.0f;
        }
        else
        {
            physics.IsGrounded = false;

            // Accumulate gravity.
            physics.Velocity.y += Gravity * physics.GravityScale * deltaTime;

            // Terminal velocity (~-55 m/s in TP, feels snappy without being instant).
            if (physics.Velocity.y < -55.0f)
                physics.Velocity.y = -55.0f;
        }

        // Apply vertical velocity.
        position.y += physics.Velocity.y * deltaTime;
    }
}
