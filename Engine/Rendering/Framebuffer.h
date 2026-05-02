#pragma once

#include <glad/glad.h>
#include <cstdint>

namespace Okari
{
	class Framebuffer
	{
	public:
		Framebuffer(uint32_t width, uint32_t height);
		~Framebuffer();

		void Bind();
		void Unbind();

		void Resize(uint32_t width, uint32_t height);

		void InitPickingPBOs();
		void RequestPickRead(uint32_t x, uint32_t y);
		bool PollPickResult(uint32_t& outID);

		void SetDrawAttachment(uint32_t attachmentIndex);
		void ClearPickingAttachment();
		uint32_t ReadPixelID(uint32_t x, uint32_t y);

		uint32_t GetPickingAttachment() const { return m_PickingAttachment; }
		uint32_t GetColorAttachment() const { return m_ColorAttachment; }
		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

	private:
		void Invalidate();

	private:
		uint32_t m_RendererID = 0;
		uint32_t m_ColorAttachment = 0;
		uint32_t m_DepthAttachment = 0;
		uint32_t m_PickingAttachment = 0;

		GLsync m_PickingFence = nullptr;
		uint32_t m_PickingPBOs[2] = { 0, 0 };
		uint32_t m_CurrentPickingPBO = 0;
		bool m_HasPendingPickRead = false;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
	};
}