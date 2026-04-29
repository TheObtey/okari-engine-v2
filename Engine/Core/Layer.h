#pragma once

#include "Rendering/Renderer.h"

namespace Okari
{
	class Layer
	{
	public:
		virtual ~Layer() = default;

		virtual void Init() {};
		virtual void Update(float deltaTime) {};
		virtual void Render(Renderer& renderer) {};
	};
}