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
		void DrawCreateFolderPopup();
		void DrawRenamePopup();

		void CreateFolder(const std::string& name);
		void RenameEntry(const std::filesystem::path& path, const std::string& newName);
		void DeleteEntry(const std::filesystem::path& path);
		void MoveEntry(const std::filesystem::path& source, const std::filesystem::path& destinationDirectory);

		bool IsInsideRoot(const std::filesystem::path& path) const;

	private:
		std::shared_ptr<Texture2D> m_FolderIcon;
		std::shared_ptr<Texture2D> m_FileIcon;

		std::filesystem::path m_RootDirectory;
		std::filesystem::path m_CurrentDirectory;

		std::filesystem::path m_SelectedEntry;
		std::filesystem::path m_EntryToRename;
		std::filesystem::path m_EntryToDelete;
		
		char m_CreateFolderBuffer[128] = "New Folder";
		char m_RenameBuffer[128] = "";

		bool m_ShouldOpenCreateFolderPopup = false;
		bool m_ShouldOpenRenamePopup = false;
		bool m_ShouldDeleteEntry = false;

		std::function<void(const std::string&)> m_OnSceneOpenRequested;
	};
}