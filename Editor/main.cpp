#include "Core/Application.h"
#include "Editor/EditorLayer.h"

#include <memory>

int main()
{
	Okari::Application app("Okari Editor");
	app.SetLayer(std::make_unique<Okari::EditorLayer>());
	app.Run();

	return 0;
}