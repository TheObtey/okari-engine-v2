#include "Rendering/Framebuffer.h"

#include <glad/glad.h>
#include <iostream>

namespace Okari
{
	Framebuffer::Framebuffer(uint32_t width, uint32_t height)
		: m_Width(width), m_Height(height)
	{
		Invalidate();
	}

	Framebuffer::~Framebuffer()
	{
		glDeleteFramebuffers(1, &m_RendererID);
		glDeleteTextures(1, &m_ColorAttachment);
		glDeleteRenderbuffers(1, &m_DepthAttachment);
		glDeleteTextures(1, &m_PickingAttachment);
		glDeleteBuffers(2, m_PickingPBOs);

		if (m_PickingFence)
		{
			glDeleteSync(m_PickingFence);
			m_PickingFence = nullptr;
		}
	}

	void Framebuffer::Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
		glViewport(0, 0, m_Width, m_Height);
	}

	void Framebuffer::Unbind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void Framebuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;

		if (m_Width == width && m_Height == height)
			return;

		m_Width = width;
		m_Height = height;

		Invalidate();
	}

	void Framebuffer::Invalidate()
	{
		if (m_RendererID)
		{
			glDeleteFramebuffers(1, &m_RendererID);
			glDeleteTextures(1, &m_ColorAttachment);
			glDeleteRenderbuffers(1, &m_DepthAttachment);
			glDeleteTextures(1, &m_PickingAttachment);
		}

		glGenFramebuffers(1, &m_RendererID);
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

		glGenTextures(1, &m_ColorAttachment);
		glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

		glGenTextures(1, &m_PickingAttachment);
		glBindTexture(GL_TEXTURE_2D, m_PickingAttachment);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, m_Width, m_Height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_PickingAttachment, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

		glGenRenderbuffers(1, &m_DepthAttachment);
		glBindRenderbuffer(GL_RENDERBUFFER, m_DepthAttachment);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_Width, m_Height);

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthAttachment);

		glDrawBuffer(GL_COLOR_ATTACHMENT0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cerr << "Framebuffer is incomplete!" << std::endl;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		InitPickingPBOs();
	}

	void Framebuffer::InitPickingPBOs()
	{
		if (m_PickingPBOs[0] != 0)
			glDeleteBuffers(2, m_PickingPBOs);

		glGenBuffers(2, m_PickingPBOs);

		for (int i = 0; i < 2; i++)
		{
			glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PickingPBOs[i]);
			glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(uint32_t), nullptr, GL_STREAM_READ);
		}

		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		m_CurrentPickingPBO = 0;
		m_HasPendingPickRead = false;
	}

	void Framebuffer::RequestPickRead(uint32_t x, uint32_t y)
	{
		GLint previousReadFramebuffer = 0;
		GLint previousDrawFramebuffer = 0;
		GLint previousReadBuffer = 0;
		GLint previousPixelPackBuffer = 0;

		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
		glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
		glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previousPixelPackBuffer);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_RendererID);
		glReadBuffer(GL_COLOR_ATTACHMENT1);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PickingPBOs[m_CurrentPickingPBO]);

		glReadPixels( x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

		if (m_PickingFence)
		{
			glDeleteSync(m_PickingFence);
			m_PickingFence = nullptr;
		}

		m_PickingFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, previousPixelPackBuffer);
		glReadBuffer(previousReadBuffer);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFramebuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previousDrawFramebuffer);

		m_HasPendingPickRead = true;
		m_CurrentPickingPBO = (m_CurrentPickingPBO + 1) % 2;
	}

	bool Framebuffer::PollPickResult(uint32_t& outID)
	{
		if (!m_HasPendingPickRead || !m_PickingFence)
			return false;

		GLenum result = glClientWaitSync(m_PickingFence, 0, 0);

		if (result == GL_TIMEOUT_EXPIRED)
			return false;

		glDeleteSync(m_PickingFence);
		m_PickingFence = nullptr;

		uint32_t readPBO = (m_CurrentPickingPBO + 1) % 2;

		GLint previousPixelPackBuffer = 0;
		glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previousPixelPackBuffer);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PickingPBOs[readPBO]);

		void* data = glMapBufferRange( GL_PIXEL_PACK_BUFFER, 0, sizeof(uint32_t), GL_MAP_READ_BIT);

		if (!data)
		{
			glBindBuffer(GL_PIXEL_PACK_BUFFER, previousPixelPackBuffer);
			return false;
		}

		outID = *reinterpret_cast<uint32_t*>(data);

		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, previousPixelPackBuffer);

		m_HasPendingPickRead = false;
		return true;
	}

	void Framebuffer::SetDrawAttachment(uint32_t attachmentIndex)
	{
		glDrawBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);
	}

	void Framebuffer::ClearPickingAttachment()
	{
		SetDrawAttachment(1);

		const GLuint clearValue = 0;
		glClearBufferuiv(GL_COLOR, 0, &clearValue);
	}

	uint32_t Framebuffer::ReadPixelID(uint32_t x, uint32_t y)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
		glReadBuffer(GL_COLOR_ATTACHMENT1);

		uint32_t pixel = 0;
		glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &pixel);

		glReadBuffer(GL_COLOR_ATTACHMENT0);

		return pixel;
	}
}