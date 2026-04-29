#define STB_IMAGE_IMPLEMENTATION

#include "Rendering/Texture2D.h"
#include <glad/glad.h>
#include <stb_image.h>
#include <iostream>

namespace Okari
{
	Texture2D::Texture2D(const std::string& path)
	{
		stbi_set_flip_vertically_on_load(true);

		unsigned char* data = stbi_load(path.c_str(), &m_Width, &m_Height, &m_Channels, 0);

		if (!data)
		{
			std::cerr << "Failed to load texture: " << path << std::endl;
			return;
		}

		GLenum format = GL_RGB;

		if (m_Channels == 1)
			format = GL_RED;
		else if (m_Channels == 3)
			format = GL_RGB;
		else if (m_Channels == 4)
			format = GL_RGBA;

		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			format,
			m_Width,
			m_Height,
			0,
			format,
			GL_UNSIGNED_BYTE,
			data
		);

		glGenerateMipmap(GL_TEXTURE_2D);

		stbi_image_free(data);
	}

	Texture2D::~Texture2D()
	{
		if (m_RendererID != 0)
			glDeleteTextures(1, &m_RendererID);
	}

	void Texture2D::Bind(unsigned int slot) const
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
	}

	void Texture2D::Unbind() const
	{
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}