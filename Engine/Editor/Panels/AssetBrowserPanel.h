#pragma once

#include "../../Rendering/Texture2D.h"

#include <filesystem>
#include <functional>

namespace Okari
{
	class AssetBrowserPanel
	{
	public:
		AssetBrowserPanel();

		void SetSceneOpenCallback(const std::function<void(const std::string&)>& callback);

		void OnImGuiRender();

	private:
		void DrawHeader();
		void DrawContent();

	private:
		std::shared_ptr<Texture2D> m_FolderIcon;
		std::shared_ptr<Texture2D> m_FileIcon;

		std::filesystem::path m_RootDirectory;
		std::filesystem::path m_CurrentDirectory;

		std::function<void(const std::string&)> m_OnSceneOpenRequested;
	};
}