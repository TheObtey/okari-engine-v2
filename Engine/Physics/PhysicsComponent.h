#pragma once

#include <glm.hpp>

namespace Okari
{
    struct PhysicsComponent
    {
        // Enable/disable gravity for this actor.
        bool UseGravity = true;

        // Current velocity in world space (metres/second).
        glm::vec3 Velocity = glm::vec3(0.0f);

        // True when the actor is standing on solid ground.
        bool IsGrounded = false;

        // Gravity acceleration (m/s^2). Twilight Princess uses a strong, snappy gravity
        // so objects fall quickly and feel weighty without floating.
        float GravityScale = 1.0f;

        // Half-height of the actor's collision capsule used for ground detection.
        // The ground ray is cast downward from (position + capsuleHalfHeight) so it
        // originates at the centre of the capsule, not the feet.
        float CapsuleHalfHeight = 0.5f;

        // Maximum distance below the actor's feet considered as "ground".
        // A small snap distance keeps the actor flush against sloped surfaces
        // without teleporting when walking off edges.
        float GroundSnapDistance = 0.15f;

        // Maximum slope angle (degrees) the actor can stand on.
        float MaxSlopeAngle = 46.0f;
    };
}
