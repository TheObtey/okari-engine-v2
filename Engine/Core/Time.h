#pragma once

namespace Okari
{
	class Time
	{
	public:
		static void Update();

		static float GetDeltaTime() { return s_DeltaTime; }
		static float GetTime() { return s_Time; }

	private:
		static float s_DeltaTime;
		static float s_Time;
		static float s_LastFrameTime;
	};
}