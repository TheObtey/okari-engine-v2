#include "Core/Application.h"
#include "GameLayer.h"

int main()
{
	Okari::Application app("Okari Sandbox");
	app.SetLayer(std::make_unique<Okari::GameLayer>());
	app.Run();

	return 0;
}