#pragma once

#include <string>

namespace Okari
{
	class Texture2D
	{
	public:
		Texture2D(const std::string& path);
		~Texture2D();

		void Bind(unsigned int slot = 0) const;
		void Unbind() const;

		unsigned int GetRendererID() { return m_RendererID; }

	private:
		unsigned int m_RendererID = 0;
		int m_Width = 0;
		int m_Height = 0;
		int m_Channels = 0;
	};
}