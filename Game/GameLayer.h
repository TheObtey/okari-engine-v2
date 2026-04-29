#pragma once

#include "Core/Layer.h"
#include "Rendering/Camera.h"
#include "World/World.h"
#include "Actors/PlayerActor.h"

#include <memory>

namespace Okari
{
	class GameLayer : public Layer
	{
	public:
		GameLayer();
		~GameLayer() override;

		void Init() override;
		void Update(float deltaTime) override;
		void Render(Renderer& renderer) override;

	private:
		std::unique_ptr<Camera> m_Camera;
		std::unique_ptr<World> m_World;

		PlayerActor m_Player;
	};
}