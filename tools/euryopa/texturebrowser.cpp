#include "euryopa.h"
#include <vector>
#include <string>

struct TextureEntry
{
	std::string name;
	rw::Texture *texture;
	std::string txdName;
};

static std::vector<TextureEntry> g_textureList;
static char g_textureSearch[128] = "";
static int g_selectedTextureIdx = -1;

static void
RefreshTextureList(void)
{
	g_textureList.clear();

	for(int i = 0; i < numTxds; i++){
		TxdDef *td = GetTxdDef(i);
		if(td == nil || td->txd == nil)
			continue;

		const char *txdName = td->name;
		if(txdName == nil || txdName[0] == '\0')
			txdName = "unknown";

		FORLIST(lnk, td->txd->textures){
			rw::Texture *tex = rw::Texture::fromDict(lnk);
			if(tex == nil)
				continue;

			TextureEntry entry;
			entry.name = tex->name;
			entry.texture = tex;
			entry.txdName = txdName;
			g_textureList.push_back(entry);
		}
	}
}

void
uiTextureBrowserWindow(void)
{
	if(!ImGui::Begin(ICON_FA_IMAGE " Texture Browser", &showTextureBrowserWindow)){
		ImGui::End();
		return;
	}

	if(g_textureList.empty()){
		if(ImGui::Button("Load Textures")){
			RefreshTextureList();
		}
		ImGui::SameLine();
		if(ImGui::Button("Refresh")){
			RefreshTextureList();
		}
		ImGui::Text("No textures loaded. Click 'Load Textures' to browse.");
		ImGui::End();
		return;
	}

	ImGui::PushItemWidth(-1);
	ImGui::InputText("Search", g_textureSearch, sizeof(g_textureSearch));
	ImGui::PopItemWidth();

	ImGui::Separator();

	float panelWidth = ImGui::GetWindowWidth() * 0.35f;
	float previewWidth = ImGui::GetWindowWidth() - panelWidth - ImGui::GetStyle().ItemSpacing.x;

	ImGui::BeginChild("##TextureList", ImVec2(panelWidth - 10, -30), true);
	ImGui::Text("Textures: %d", (int)g_textureList.size());
	ImGui::BeginChild("##TextureScrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
	for(size_t i = 0; i < g_textureList.size(); i++){
		TextureEntry &entry = g_textureList[i];

		bool matchSearch = g_textureSearch[0] == '\0' ||
			strstr(entry.name.c_str(), g_textureSearch) != nil ||
			strstr(entry.txdName.c_str(), g_textureSearch) != nil;

		if(!matchSearch)
			continue;

		bool isSelected = (int)i == g_selectedTextureIdx;
		ImGui::PushID((int)i);
		if(ImGui::Selectable(entry.name.c_str(), isSelected, 0, ImVec2(panelWidth - 30, 0))){
			g_selectedTextureIdx = (int)i;
		}
		if(ImGui::IsItemHovered()){
			ImGui::SetTooltip("TXD: %s", entry.txdName.c_str());
		}
		ImGui::PopID();
	}
	ImGui::EndChild();
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("##TexturePreview", ImVec2(previewWidth - 10, -30), true);
	if(g_selectedTextureIdx >= 0 && g_selectedTextureIdx < (int)g_textureList.size()){
		TextureEntry &entry = g_textureList[g_selectedTextureIdx];

		ImGui::Text("Name: %s", entry.name.c_str());
		ImGui::Text("TXD: %s", entry.txdName.c_str());

		if(entry.texture){
			rw::Raster *raster = entry.texture->raster;
			if(raster){
				int width = raster->width;
				int height = raster->height;
				ImGui::Text("Size: %dx%d", width, height);

				float maxPreviewSize = 256.0f;
				float aspectRatio = (float)width / (float)height;
				float previewW, previewH;
				if(width > height){
					previewW = maxPreviewSize;
					previewH = maxPreviewSize / aspectRatio;
				}else{
					previewH = maxPreviewSize;
					previewW = maxPreviewSize * aspectRatio;
				}

				ImGui::Image((ImTextureID)entry.texture, ImVec2(previewW, previewH));
			}else{
				ImGui::Text("No raster data");
			}
		}
	}else{
		ImGui::Text("Select a texture to preview");
	}
	ImGui::EndChild();

	ImGui::Separator();

	ImGui::Text("Total: %d textures", (int)g_textureList.size());
	if(ImGui::Button("Refresh List")){
		RefreshTextureList();
	}
	ImGui::SameLine();
	if(ImGui::Button("Clear")){
		g_textureList.clear();
		g_selectedTextureIdx = -1;
	}

	ImGui::End();
}

rw::Texture*
GetSelectedTexture(void)
{
	if(g_selectedTextureIdx >= 0 && g_selectedTextureIdx < (int)g_textureList.size()){
		return g_textureList[g_selectedTextureIdx].texture;
	}
	return nil;
}

const char*
GetSelectedTextureName(void)
{
	if(g_selectedTextureIdx >= 0 && g_selectedTextureIdx < (int)g_textureList.size()){
		return g_textureList[g_selectedTextureIdx].name.c_str();
	}
	return nil;
}
