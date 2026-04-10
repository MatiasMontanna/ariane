#include "euryopa.h"
#include "cars.h"
#include "modloader.h"

static std::vector<CarSpawn> carSpawns;

bool Cars::gRenderAsCubes = true;
bool Cars::gRenderVehicleId = false;
bool Cars::gRenderPrimaryColor = false;
bool Cars::gRenderSecondaryColor = false;
bool Cars::gRenderForceSpawn = false;
bool Cars::gRenderAlarmProb = false;
bool Cars::gRenderLockedProb = false;
bool Cars::gRenderUnknown1 = false;
bool Cars::gRenderUnknown2 = false;

namespace Cars {

void
Init(void)
{
	carSpawns.clear();

	char path[256];
	snprintf(path, sizeof(path), "data/binary/ipl/*");

	log("Cars: searching for .ipl files in %s\n", path);

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(path, &findData);
	if(hFind == INVALID_HANDLE_VALUE){
		log("Cars: data/binary/ipl folder not found\n");
		return;
	}

	do{
		const char *filename = findData.cFileName;
		size_t namelen = strlen(filename);
		if(namelen < 4 || strcmp(filename + namelen - 4, ".ipl") != 0)
			continue;

		char filepath[256];
		snprintf(filepath, sizeof(filepath), "data/binary/ipl/%s", filename);

		int size;
		uint8 *buf = ReadLooseFile(filepath, &size);
		if(buf == nil){
			log("Cars: failed to read %s\n", filepath);
			continue;
		}

		if(size < 0x4C){
			log("Cars: %s too small (%d bytes)\n", filename, size);
			free(buf);
			continue;
		}

		uint32 magic = *(uint32*)buf;
		if(magic != 0x79726E62){
			log("Cars: %s not bnry format (magic=0x%X)\n", filename, magic);
			free(buf);
			continue;
		}

		int32 numCars = *(int32*)(buf + 0x14);
		log("Cars: %s has %d parked cars\n", filename, numCars);

		if(numCars <= 0){
			free(buf);
			continue;
		}

		int32 carsOffset = *(int32*)(buf + 0x3C);
		if(carsOffset <= 0 || carsOffset + numCars * 48 > size){
			log("Cars: %s invalid offset/size\n", filename);
			free(buf);
			continue;
		}

		log("Cars: parsing %d car spawns from %s\n", numCars, filename);

		uint8 *carsData = buf + carsOffset;
		for(int i = 0; i < numCars; i++){
			uint8 *carEntry = carsData + i * 48;

			CarSpawn spawn;
			spawn.x = *(float*)(carEntry + 0);
			spawn.y = *(float*)(carEntry + 4);
			spawn.z = *(float*)(carEntry + 8);
			spawn.angle = *(float*)(carEntry + 12);
			spawn.vehicleId = *(int32*)(carEntry + 16);
			spawn.primaryColor = *(int32*)(carEntry + 20);
			spawn.secondaryColor = *(int32*)(carEntry + 24);
			spawn.forceSpawn = *(int32*)(carEntry + 28);
			spawn.alarmProb = *(int32*)(carEntry + 32);
			spawn.lockedProb = *(int32*)(carEntry + 36);
			spawn.unknown1 = *(int32*)(carEntry + 40);
			spawn.unknown2 = *(int32*)(carEntry + 44);

			carSpawns.push_back(spawn);
		}

		free(buf);
	}while(FindNextFileA(hFind, &findData));

	FindClose(hFind);

	log("Cars: loaded %d spawns\n", (int)carSpawns.size());
}

bool
HasCarSpawns(void)
{
	return !carSpawns.empty();
}

void
Render(void)
{
	if(carSpawns.empty())
		return;

	if(!Cars::gRenderAsCubes)
		return;

	uint8 alpha = (uint8)(gCollisionWireframeAlpha * 255.0f);
	rw::RGBA col = { 0, 255, 0, alpha };

	for(size_t i = 0; i < carSpawns.size(); i++){
		CarSpawn &car = carSpawns[i];

		float halfX = 1.5f;
		float halfY = 1.5f;
		float halfZ = 1.5f;

		rw::V3d v[8] = {
			{ car.x - halfX, car.y - halfY, car.z },
			{ car.x + halfX, car.y - halfY, car.z },
			{ car.x - halfX, car.y + halfY, car.z },
			{ car.x + halfX, car.y + halfY, car.z },
			{ car.x - halfX, car.y - halfY, car.z + halfZ * 2.0f },
			{ car.x + halfX, car.y - halfY, car.z + halfZ * 2.0f },
			{ car.x - halfX, car.y + halfY, car.z + halfZ * 2.0f },
			{ car.x + halfX, car.y + halfY, car.z + halfZ * 2.0f }
		};

		RenderLine(v[0], v[1], col, col);
		RenderLine(v[2], v[3], col, col);
		RenderLine(v[4], v[5], col, col);
		RenderLine(v[6], v[7], col, col);
		RenderLine(v[0], v[2], col, col);
		RenderLine(v[1], v[3], col, col);
		RenderLine(v[4], v[6], col, col);
		RenderLine(v[5], v[7], col, col);
		RenderLine(v[0], v[4], col, col);
		RenderLine(v[1], v[5], col, col);
		RenderLine(v[2], v[6], col, col);
		RenderLine(v[3], v[7], col, col);

		if(!Cars::gRenderVehicleId && !Cars::gRenderPrimaryColor && !Cars::gRenderSecondaryColor &&
		   !Cars::gRenderForceSpawn && !Cars::gRenderAlarmProb && !Cars::gRenderLockedProb &&
		   !Cars::gRenderUnknown1 && !Cars::gRenderUnknown2)
			continue;

		rw::V3d worldPos = { car.x, car.y, car.z + halfZ * 2.5f };
		rw::V3d screenPos;
		float w, h;
		if(Sprite::CalcScreenCoors(worldPos, &screenPos, &w, &h, false)){
			if(screenPos.z > 0.0f && screenPos.z < gTextFarClip){
				char label[256];
				label[0] = '\0';
				if(Cars::gRenderVehicleId){
					strncat(label, "ID:", sizeof(label) - strlen(label) - 1);
					char tmp[32];
					snprintf(tmp, sizeof(tmp), "%d ", car.vehicleId);
					strncat(label, tmp, sizeof(label) - strlen(label) - 1);
				}
				if(Cars::gRenderPrimaryColor){
					strncat(label, "P:", sizeof(label) - strlen(label) - 1);
					char tmp[32];
					snprintf(tmp, sizeof(tmp), "%d ", car.primaryColor);
					strncat(label, tmp, sizeof(label) - strlen(label) - 1);
				}
				if(Cars::gRenderSecondaryColor){
					strncat(label, "S:", sizeof(label) - strlen(label) - 1);
					char tmp[32];
					snprintf(tmp, sizeof(tmp), "%d ", car.secondaryColor);
					strncat(label, tmp, sizeof(label) - strlen(label) - 1);
				}
				if(Cars::gRenderForceSpawn){
					strncat(label, "FS:", sizeof(label) - strlen(label) - 1);
					char tmp[32];
					snprintf(tmp, sizeof(tmp), "%d ", car.forceSpawn);
					strncat(label, tmp, sizeof(label) - strlen(label) - 1);
				}
				if(Cars::gRenderAlarmProb){
					strncat(label, "AP:", sizeof(label) - strlen(label) - 1);
					char tmp[32];
					snprintf(tmp, sizeof(tmp), "%d ", car.alarmProb);
					strncat(label, tmp, sizeof(label) - strlen(label) - 1);
				}
				if(Cars::gRenderLockedProb){
					strncat(label, "LP:", sizeof(label) - strlen(label) - 1);
					char tmp[32];
					snprintf(tmp, sizeof(tmp), "%d ", car.lockedProb);
					strncat(label, tmp, sizeof(label) - strlen(label) - 1);
				}
				if(Cars::gRenderUnknown1){
					strncat(label, "U1:", sizeof(label) - strlen(label) - 1);
					char tmp[32];
					snprintf(tmp, sizeof(tmp), "%d ", car.unknown1);
					strncat(label, tmp, sizeof(label) - strlen(label) - 1);
				}
				if(Cars::gRenderUnknown2){
					strncat(label, "U2:", sizeof(label) - strlen(label) - 1);
					char tmp[32];
					snprintf(tmp, sizeof(tmp), "%d ", car.unknown2);
					strncat(label, tmp, sizeof(label) - strlen(label) - 1);
				}

				if(label[0] != '\0'){
					ImU32 labelCol = IM_COL32(255, 255, 0, 255);
					ImFont* font = ImGui::GetFont();
					float fontSize = ImGui::GetFontSize();
					ImVec2 textSize = ImGui::CalcTextSize(label);
					float x = screenPos.x - textSize.x * 0.5f;
					float y = screenPos.y - textSize.y;
					ImDrawList* drawList = ImGui::GetBackgroundDrawList();
					drawList->AddText(font, fontSize, ImVec2(x + 1.0f, y + 1.0f), IM_COL32_BLACK, label);
					drawList->AddText(font, fontSize, ImVec2(x, y), labelCol, label);
				}
			}
		}
	}
}

}  // namespace Cars