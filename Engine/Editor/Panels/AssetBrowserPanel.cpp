#include "AssetBrowserPanel.h"

#include <imgui.h>

namespace Okari
{
	AssetBrowserPanel::AssetBrowserPanel()
	{
		std::string iconPath = std::string(OKARI_ASSET_DIR) + "/Editor/Icons/";

		m_FolderIcon = std::make_shared<Texture2D>(iconPath + "folder.png");
		m_FileIcon = std::make_shared<Texture2D>(iconPath + "file.png");

		m_RootDirectory = std::filesystem::path(OKARI_ASSET_DIR);
		m_CurrentDirectory = m_RootDirectory;
	}

	void AssetBrowserPanel::SetSceneOpenCallback(const std::function<void(const std::string&)>& callback)
	{
		m_OnSceneOpenRequested = callback;
	}

	void AssetBrowserPanel::OnImGuiRender()
	{
		ImGui::Begin("Assets");

		DrawHeader();

		ImGui::Separator();

		DrawContent();
		DrawCreateFolderPopup();
		DrawRenamePopup();

		ImGui::End();
	}

	void AssetBrowserPanel::DrawHeader()
	{
		bool canGoBack = m_CurrentDirectory != m_RootDirectory;

		if (!canGoBack)
			ImGui::BeginDisabled();

		if (ImGui::Button("<-"))
		{
			if (canGoBack)
				m_CurrentDirectory = m_CurrentDirectory.parent_path();
		}

		if (!canGoBack)
			ImGui::EndDisabled();

		ImGui::SameLine();

		std::filesystem::path relativePath = std::filesystem::relative(m_CurrentDirectory, m_RootDirectory);

		if (relativePath.empty() || relativePath == ".")
			ImGui::Text("Assets/");
		else
			ImGui::Text("Assets/%s", relativePath.generic_string().c_str());
	}

	void AssetBrowserPanel::DrawContent()
	{
		ImGui::BeginChild("AssetBrowserScroll", ImVec2(0, 0), true);

		const float cellSize = 110.0f;
		const float iconSize = 56.0f;

		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = static_cast<int>(panelWidth / cellSize);

		if (columnCount < 1)
			columnCount = 1;

		if (ImGui::BeginTable("AssetBrowserTable", columnCount))
		{
			for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
			{
				const auto& path = entry.path();
				std::string filename = path.filename().string();

				bool isDirectory = entry.is_directory();

				ImGui::TableNextColumn();

				ImGui::PushID(path.string().c_str());

				ImGui::BeginGroup();

				float cursorX = ImGui::GetCursorPosX();
				ImGui::SetCursorPosX(cursorX + (cellSize - iconSize) * 0.5f);

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));

				auto texture = isDirectory ? m_FolderIcon : m_FileIcon;

				ImGui::ImageButton(
					"##AssetIcon",
					(ImTextureID)(intptr_t)texture->GetRendererID(),
					ImVec2(iconSize, iconSize),
					ImVec2(0, 1),
					ImVec2(1, 0)
				);

				ImGui::PopStyleColor(3);

				if (ImGui::IsItemHovered())
				{
					ImGui::GetWindowDrawList()->AddRect(
						ImGui::GetItemRectMin(),
						ImGui::GetItemRectMax(),
						IM_COL32(255, 255, 255, 80),
						4.0f
					);
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (isDirectory)
						m_CurrentDirectory = path;
					else if (path.extension() == ".okscene")
					{
						if (m_OnSceneOpenRequested)
							m_OnSceneOpenRequested(path.string());
					}
				}

				float textWidth = ImGui::CalcTextSize(filename.c_str()).x;
				float textX = ImGui::GetCursorPosX() + (cellSize - textWidth) * 0.5f;

				if (textWidth < cellSize)
					ImGui::SetCursorPosX(textX);

				ImGui::TextWrapped("%s", filename.c_str());

				ImGui::EndGroup();

				if (ImGui::BeginPopupContextItem("AssetContextMenu"))
				{
					if (ImGui::MenuItem("Rename"))
					{
						m_EntryToRename = path;

						std::string filename = path.filename().string();
						std::snprintf(m_RenameBuffer, sizeof(m_RenameBuffer), "%s", filename.c_str());

						m_ShouldOpenRenamePopup = true;
					}

					if (ImGui::MenuItem("Delete"))
					{
						m_EntryToDelete = path;
						m_ShouldDeleteEntry = true;
					}

					ImGui::EndPopup();
				}

				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		if (ImGui::BeginPopupContextWindow("AssetBrowserEmptyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Folder"))
			{
				std::snprintf(m_CreateFolderBuffer, sizeof(m_CreateFolderBuffer), "New Folder");
				m_ShouldOpenCreateFolderPopup = true;
			}

			ImGui::EndPopup();
		}

		if (m_ShouldDeleteEntry)
		{
			DeleteEntry(m_EntryToDelete);

			m_EntryToDelete.clear();
			m_ShouldDeleteEntry = false;
		}

		ImGui::EndChild();
	}

	void AssetBrowserPanel::DrawCreateFolderPopup()
	{
		if (m_ShouldOpenCreateFolderPopup)
		{
			ImGui::OpenPopup("Create Folder");
			m_ShouldOpenCreateFolderPopup = false;
		}

		if (ImGui::BeginPopupModal("Create Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::InputText("Name", m_CreateFolderBuffer, sizeof(m_CreateFolderBuffer));

			ImGui::Separator();

			if (ImGui::Button("Create"))
			{
				CreateFolder(m_CreateFolderBuffer);
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	void AssetBrowserPanel::DrawRenamePopup()
	{
		if (m_ShouldOpenRenamePopup)
		{
			ImGui::OpenPopup("Rename");
			m_ShouldOpenRenamePopup = false;
		}

		if (ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::InputText("New Name", m_RenameBuffer, sizeof(m_RenameBuffer));

			ImGui::Separator();

			if (ImGui::Button("Rename"))
			{
				RenameEntry(m_EntryToRename, m_RenameBuffer);
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	bool AssetBrowserPanel::IsInsideRoot(const std::filesystem::path& path) const
	{
		auto normalizedRoot = std::filesystem::weakly_canonical(m_RootDirectory);
		auto normalizedPath = std::filesystem::weakly_canonical(path);

		auto rootStr = normalizedRoot.string();
		auto pathStr = normalizedPath.string();

		return pathStr.rfind(rootStr, 0) == 0;
	}

	void AssetBrowserPanel::CreateFolder(const std::string& name)
	{
		if (name.empty())
			return;

		std::filesystem::path folderPath = m_CurrentDirectory / name;

		if (!IsInsideRoot(folderPath))
			return;

		std::error_code ec;
		std::filesystem::create_directory(folderPath, ec);
	}

	void AssetBrowserPanel::RenameEntry(const std::filesystem::path& path, const std::string& newName)
	{
		if (newName.empty())
			return;

		if (!IsInsideRoot(path))
			return;

		std::filesystem::path newPath = path.parent_path() / newName;

		if (!IsInsideRoot(newPath))
			return;

		std::error_code ec;
		std::filesystem::rename(path, newPath, ec);
	}

	void AssetBrowserPanel::DeleteEntry(const std::filesystem::path& path)
	{
		if (!IsInsideRoot(path))
			return;

		std::error_code ec;

		if (std::filesystem::is_directory(path))
			std::filesystem::remove_all(path, ec);
		else
			std::filesystem::remove(path, ec);
	}

	void AssetBrowserPanel::MoveEntry(const std::filesystem::path& source, const std::filesystem::path& destinationDirectory)
	{
		if (!IsInsideRoot(source) || !IsInsideRoot(destinationDirectory))
			return;

		if (!std::filesystem::is_directory(destinationDirectory))
			return;

		std::filesystem::path destination = destinationDirectory / source.filename();

		if (!IsInsideRoot(destination))
			return;

		if (source == destination)
			return;

		std::error_code ec;
		std::filesystem::rename(source, destination, ec);
	}
}