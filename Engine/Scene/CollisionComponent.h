#pragma once

#include "Scene/Transform.h"
#include "Collision/CollisionTypes.h"

#include <string>

namespace Okari
{
    struct CollisionComponent
    {
        bool Enabled = true;

        std::string CollisionPath;

        // Local transform relative to the owning object's transform.
        // The final world-space collision transform is: World::GetWorldMatrix(ownerID) * CollisionOffset.GetModelMatrix()
        Transform CollisionOffset;

        // Populated at runtime by World::BuildCollisionWorld().
        // Not serialized.
        CollisionMesh RuntimeMesh;
    };
}
