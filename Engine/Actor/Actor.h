#pragma once

#include "../Scene/WorldObject.h"

namespace Okari
{
	class Actor
	{
	public:
		explicit Actor(WorldObject* object)
			: m_Object(object)
		{ }

		virtual ~Actor() = default;

		virtual void OnCreate() {}
		virtual void OnUpdate(float deltaTime) {}
		virtual void OnDestroy() {}

		WorldObject* GetObject() { return m_Object; }
		const WorldObject* GetObject() const { return m_Object; }

	protected:
		WorldObject* m_Object = nullptr;
	};
}