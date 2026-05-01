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

	void AssetBrowserPanel::OnImGuiRender()
	{
		ImGui::Begin("Assets");

		DrawHeader();

		ImGui::Separator();

		DrawContent();

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

				if (isDirectory && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					m_CurrentDirectory = path;

				float textWidth = ImGui::CalcTextSize(filename.c_str()).x;
				float textX = ImGui::GetCursorPosX() + (cellSize - textWidth) * 0.5f;

				if (textWidth < cellSize)
					ImGui::SetCursorPosX(textX);

				ImGui::TextWrapped("%s", filename.c_str());

				ImGui::EndGroup();

				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		ImGui::EndChild();
	}
}