#include "Platform/Window.h"
#include "Rendering/Renderer.h"

int main()
{
    Okari::Window window(
        1280,
        720,
        "Okari Engine - J3D Viewer"
    );

    Okari::Renderer renderer;
    renderer.Init();

    while (!window.ShouldClose())
    {
        renderer.BeginFrame();

        // do shit

        renderer.EndFrame();

        window.SwapBuffers();
        window.PollEvents();
    }

    return 0;
}