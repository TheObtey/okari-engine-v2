#include "J3DPreviewMeshBuilder.h"

#include "Formats/J3D/J3DModelLoader.h"
#include "Platform/Window.h"
#include "Rendering/Camera.h"
#include "Rendering/Renderer.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include <iostream>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr
            << "Usage: J3DViewer <model.bmd|model.bdl>\n";

        return 1;
    }

    Okari::Window window(
        1280,
        720,
        "Okari Engine - J3D Viewer"
    );

    Okari::Renderer renderer;
    renderer.Init();

    Okari::J3DModelLoadResult loadResult =
        Okari::J3DModelLoader::Load(argv[1]);

    if (!loadResult.Succeeded())
    {
        std::cerr
            << "Failed to load J3D model:\n"
            << loadResult.Error
            << '\n';

        return 1;
    }

    Okari::J3DPreviewMeshBuildResult previewResult =
        Okari::J3DPreviewMeshBuilder::Build(
            loadResult.Model
        );

    if (!previewResult.Succeeded())
    {
        std::cerr
            << "Failed to build preview mesh:\n"
            << previewResult.Error
            << '\n';

        return 1;
    }

    std::cout
        << "J3D model loaded successfully\n"
        << "Triangles: "
        << loadResult.Model.TriangleGeometry.TriangleCount()
        << '\n'
        << "Preview vertices: "
        << previewResult.PreviewMesh->GetVertexCount()
        << '\n';

    constexpr float renderScale = 0.01f;

    const Okari::AABB& bounds =
        previewResult.PreviewMesh->GetBounds();

    const glm::vec3 center =
        ((bounds.Min + bounds.Max) * 0.5f) *
        renderScale;

    const glm::vec3 halfExtents =
        ((bounds.Max - bounds.Min) * 0.5f) *
        renderScale;

    const float radius =
        glm::length(halfExtents);

    Okari::Camera camera(
        1280.0f / 720.0f
    );

    camera.SetTarget(center);

    camera.SetPosition(
        center +
        glm::vec3(
            0.0f,
            radius * 0.15f,
            radius * 2.5f
        )
    );

    const glm::mat4 modelMatrix =
        glm::scale(
            glm::mat4(1.0f),
            glm::vec3(renderScale)
        );

    while (!window.ShouldClose())
    {
        renderer.BeginFrame();

        renderer.DrawMeshDebug(
            modelMatrix,
            previewResult.PreviewMesh.get(),
            camera,
            glm::vec3(0.75f)
        );

        renderer.EndFrame();

        window.SwapBuffers();
        window.PollEvents();
    }

    return 0;
}