#include "euryopa.h"
#include "cars.h"
#include "modloader.h"
#include <map>
#include <string>

static FILE *logFile = nil;

static void openLogFile(void){
	logFile = fopen("cars_merge.log", "w");
	if(logFile){
		fprintf(logFile, "Cars merge log started\n");
		fflush(logFile);
	}
}

static void closeLogFile(void){
	if(logFile){
		fprintf(logFile, "Cars merge log ended\n");
		fflush(logFile);
		fclose(logFile);
		logFile = nil;
	}
}

static std::vector<CarSpawn> carSpawns;
static int selectedCarSpawnIndex = -1;

bool Cars::gRenderAsCubes = true;
bool Cars::gRenderVehicleId = false;
bool Cars::gRenderPrimaryColor = false;
bool Cars::gRenderSecondaryColor = false;
bool Cars::gRenderForceSpawn = false;
bool Cars::gRenderAlarmProb = false;
bool Cars::gRenderLockedProb = false;
bool Cars::gRenderUnknown1 = false;
bool Cars::gRenderUnknown2 = false;
bool Cars::gRenderFileName = false;
bool Cars::gRenderAngle = true;
bool Cars::gReplaceWithModCars = false;
bool Cars::gAdditiveMerge = false;

namespace Cars {

void
Init(void)
{
	carSpawns.clear();

	log("Cars: Init - searching for .ipl files in data/binary/ipl/");

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
		log("Cars: reading: %s", filepath);

		int size;
		uint8 *buf = ReadLooseFile(filepath, &size);
		if(buf == nil){
			log("Cars: failed to read %s\n", filepath);
			continue;
		}

		if(size < 0x4C){
			log("Cars: %s too small (%d bytes)\n", filename, size);
			if(logFile) fprintf(logFile, "too small: %s (%d bytes)\n", filename, size);
			free(buf);
			continue;
		}

		uint32 magic = *(uint32*)buf;
		if(magic != 0x79726E62){
			log("Cars: %s not bnry format (magic=0x%X)\n", filename, magic);
			if(logFile) fprintf(logFile, "not bnry: %s magic=0x%X\n", filename, magic);
			free(buf);
			continue;
		}

		int32 numCars = *(int32*)(buf + 0x14);
		int32 carsOffset = *(int32*)(buf + 0x3C);
		log("Cars: %s has %d parked cars at offset 0x%X\n", filename, numCars, (unsigned int)carsOffset);
		if(logFile) fprintf(logFile, "%s: %d cars at offset 0x%X, size=%d\n", filename, numCars, (unsigned int)carsOffset, size);

		if(numCars <= 0){
			free(buf);
			continue;
		}

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
			strncpy(spawn.iplName, filename, sizeof(spawn.iplName) - 1);
			spawn.iplName[sizeof(spawn.iplName) - 1] = '\0';

			carSpawns.push_back(spawn);
		}

		free(buf);
	}while(FindNextFileA(hFind, &findData));

	FindClose(hFind);

	log("Cars: loaded %d spawns\n", (int)carSpawns.size());
	if(logFile) fprintf(logFile, "Cars: total loaded %d spawns\n", (int)carSpawns.size());
}

bool
HasCarSpawns(void)
{
	return !carSpawns.empty();
}

void
ExportCSV(void)
{
	if(carSpawns.empty())
		return;

	FILE *f = fopen("car_spawns_export.csv", "w");
	if(f == nil){
		log("Cars: failed to create export file\n");
		return;
	}

	fprintf(f, "X,Y,Z,Angle,Object ID,Primary colour,Secondary colour,Force spawn,Alarm probability,Locked probability,Unknown1,Unknown2,File name\n");

	for(size_t i = 0; i < carSpawns.size(); i++){
		CarSpawn &car = carSpawns[i];
		fprintf(f, "%.6f,%.6f,%.6f,%.4f,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",
			car.x, car.y, car.z, car.angle,
			car.vehicleId, car.primaryColor, car.secondaryColor,
			car.forceSpawn, car.alarmProb, car.lockedProb,
			car.unknown1, car.unknown2, car.iplName);
	}

	fclose(f);
	log("Cars: exported %d spawns to car_spawns_export.csv\n", (int)carSpawns.size());
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

		if(Cars::gRenderAngle){
			float arrowLength = 3.0f;
			float arrowSize = 0.8f;
			float cosA = cosf(car.angle);
			float sinA = sinf(car.angle);
			rw::V3d dir = { cosA * arrowLength, sinA * arrowLength, 0.0f };
			rw::V3d base = { car.x, car.y, car.z + halfZ };
			rw::V3d tip = { base.x + dir.x, base.y + dir.y, base.z + dir.z };
			rw::V3d left = { base.x + cosA * arrowSize - sinA * arrowSize, base.y + sinA * arrowSize + cosA * arrowSize, base.z };
			rw::V3d right = { base.x + cosA * arrowSize + sinA * arrowSize, base.y + sinA * arrowSize - cosA * arrowSize, base.z };
			RenderLine(base, tip, col, col);
			RenderLine(tip, left, col, col);
			RenderLine(tip, right, col, col);
		}

		if(!Cars::gRenderVehicleId && !Cars::gRenderPrimaryColor && !Cars::gRenderSecondaryColor &&
		   !Cars::gRenderForceSpawn && !Cars::gRenderAlarmProb && !Cars::gRenderLockedProb &&
		   !Cars::gRenderUnknown1 && !Cars::gRenderUnknown2 && !Cars::gRenderFileName)
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
				if(Cars::gRenderFileName){
					strncat(label, "F:", sizeof(label) - strlen(label) - 1);
					strncat(label, car.iplName, sizeof(label) - strlen(label) - 1);
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

void
RenderPicking(void)
{
	if(carSpawns.empty())
		return;

	rw::RGBA col;
	for(size_t i = 0; i < carSpawns.size(); i++){
		CarSpawn &car = carSpawns[i];

		int idx = (int)i + 1;
		col.red = (idx) & 0xFF;
		col.green = (idx >> 8) & 0xFF;
		col.blue = (idx >> 16) & 0xFF;
		col.alpha = 255;

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
	}
}

static uint32
EncodeColorCode(int idx)
{
	if(idx <= 0)
		return 0xFF000000;
	return ((idx & 0xFF) << 0) | ((idx >> 8) & 0xFF) << 8 | ((idx >> 16) & 0xFF) << 16 | 0xFF000000;
}

int
PickCarSpawn(void)
{
	if(carSpawns.empty() || !gRenderCarSpawns)
		return -1;

	rw::V3d origin = TheCamera.m_position;
	rw::V3d dir = normalize(TheCamera.m_mouseDir);
	Ray ray;
	ray.start = origin;
	ray.dir = dir;

	float bestT = 1e30f;
	int bestIdx = -1;

	for(size_t i = 0; i < carSpawns.size(); i++){
		CarSpawn &car = carSpawns[i];
		CSphere sphere;
		sphere.center = { car.x, car.y, car.z + 1.5f };
		sphere.radius = 2.5f;

		float t;
		if(IntersectRaySphere(ray, sphere, &t) && t > 0.0f && t < bestT){
			bestT = t;
			bestIdx = (int)i;
		}
	}

	return bestIdx;
}

void
SelectCarSpawn(int index)
{
	selectedCarSpawnIndex = index;
}

int
GetSelectedCarSpawnIndex(void)
{
	return selectedCarSpawnIndex;
}

CarSpawn*
GetSelectedCarSpawn(void)
{
	if(selectedCarSpawnIndex >= 0 && selectedCarSpawnIndex < (int)carSpawns.size())
		return &carSpawns[selectedCarSpawnIndex];
	return nil;
}

void
MoveSelectedCarSpawn(float x, float y, float z)
{
	if(selectedCarSpawnIndex >= 0 && selectedCarSpawnIndex < (int)carSpawns.size()){
		carSpawns[selectedCarSpawnIndex].x = x;
		carSpawns[selectedCarSpawnIndex].y = y;
		carSpawns[selectedCarSpawnIndex].z = z;
	}
}

void
RotateSelectedCarSpawn(float angle)
{
	if(selectedCarSpawnIndex >= 0 && selectedCarSpawnIndex < (int)carSpawns.size()){
		carSpawns[selectedCarSpawnIndex].angle = angle;
	}
}

void
SetSelectedCarSpawnProperty(int property, int value)
{
	if(selectedCarSpawnIndex < 0 || selectedCarSpawnIndex >= (int)carSpawns.size())
		return;
	CarSpawn &car = carSpawns[selectedCarSpawnIndex];
	switch(property){
	case 0: car.vehicleId = value; break;
	case 1: car.primaryColor = value; break;
	case 2: car.secondaryColor = value; break;
	case 3: car.forceSpawn = value; break;
	case 4: car.alarmProb = value; break;
	case 5: car.lockedProb = value; break;
	case 6: car.unknown1 = value; break;
	case 7: car.unknown2 = value; break;
	}
}

int
GetNumCarSpawns(void)
{
	return (int)carSpawns.size();
}

CarSpawn*
GetCarSpawn(int index)
{
	if(index >= 0 && index < (int)carSpawns.size())
		return &carSpawns[index];
	return nil;
}

void
SaveAllCarSpawns(void)
{
	if(carSpawns.empty())
		return;

	std::map<std::string, std::vector<int>> fileToIndices;
	for(size_t i = 0; i < carSpawns.size(); i++){
		fileToIndices[carSpawns[i].iplName].push_back((int)i);
	}

	for(auto &kv : fileToIndices){
		const char *filename = kv.first.c_str();
		const std::vector<int> &indices = kv.second;
		int numCars = (int)indices.size();

		char filepath[256];
		snprintf(filepath, sizeof(filepath), "data/binary/ipl/%s", filename);

		FILE *f = fopen(filepath, "r+b");
		if(f == nil){
			log("Cars: failed to open %s for writing\n", filepath);
			continue;
		}

		fseek(f, 0x14, SEEK_SET);
		fwrite(&numCars, sizeof(int32), 1, f);

		int32 carsOffset;
		fseek(f, 0x3C, SEEK_SET);
		fread(&carsOffset, sizeof(int32), 1, f);

		fseek(f, carsOffset, SEEK_SET);
		for(size_t i = 0; i < indices.size(); i++){
			CarSpawn &car = carSpawns[indices[i]];
			fwrite(&car.x, sizeof(float), 1, f);
			fwrite(&car.y, sizeof(float), 1, f);
			fwrite(&car.z, sizeof(float), 1, f);
			fwrite(&car.angle, sizeof(float), 1, f);
			fwrite(&car.vehicleId, sizeof(int32), 1, f);
			fwrite(&car.primaryColor, sizeof(int32), 1, f);
			fwrite(&car.secondaryColor, sizeof(int32), 1, f);
			fwrite(&car.forceSpawn, sizeof(int32), 1, f);
			fwrite(&car.alarmProb, sizeof(int32), 1, f);
			fwrite(&car.lockedProb, sizeof(int32), 1, f);
			fwrite(&car.unknown1, sizeof(int32), 1, f);
			fwrite(&car.unknown2, sizeof(int32), 1, f);
		}

		fclose(f);
		log("Cars: saved %d spawns to %s\n", numCars, filename);
	}

	Toast(TOAST_SAVE, "Saved %d car spawns to disk", (int)carSpawns.size());
}

static int
ReadCarSpawnsFromBuffer(uint8 *buf, int size, CarSpawn *spawns, int maxSpawns)
{
	if(size < 0x4C)
		return 0;

	uint32 magic = *(uint32*)buf;
	if(magic != 0x79726E62)
		return 0;

	int32 numCars = *(int32*)(buf + 0x14);
	if(numCars <= 0 || numCars > maxSpawns)
		return 0;

	int32 carsOffset = *(int32*)(buf + 0x3C);
	if(carsOffset <= 0 || carsOffset + numCars * 48 > size)
		return 0;

	uint8 *carsData = buf + carsOffset;
	for(int i = 0; i < numCars; i++){
		uint8 *carEntry = carsData + i * 48;
		spawns[i].x = *(float*)(carEntry + 0);
		spawns[i].y = *(float*)(carEntry + 4);
		spawns[i].z = *(float*)(carEntry + 8);
		spawns[i].angle = *(float*)(carEntry + 12);
		spawns[i].vehicleId = *(int32*)(carEntry + 16);
		spawns[i].primaryColor = *(int32*)(carEntry + 20);
		spawns[i].secondaryColor = *(int32*)(carEntry + 24);
		spawns[i].forceSpawn = *(int32*)(carEntry + 28);
		spawns[i].alarmProb = *(int32*)(carEntry + 32);
		spawns[i].lockedProb = *(int32*)(carEntry + 36);
		spawns[i].unknown1 = *(int32*)(carEntry + 40);
		spawns[i].unknown2 = *(int32*)(carEntry + 44);
	}

	return numCars;
}

void
MergeModCarSpawns(void)
{
	openLogFile();
	if(logFile) fprintf(logFile, "Cars merge log started\n");

	log("Cars: starting merge from data/binary/mod/");
	if(logFile) fprintf(logFile, "Cars: starting merge from data/binary/mod/\n");

	char modPath[256];
	snprintf(modPath, sizeof(modPath), "data/binary/mod");
	DWORD modAttr = GetFileAttributesA(modPath);
	if(modAttr == INVALID_FILE_ATTRIBUTES || !(modAttr & FILE_ATTRIBUTE_DIRECTORY)){
		log("Cars: data/binary/mod folder not found");
		if(logFile) fprintf(logFile, "data/binary/mod folder not found\n");
		closeLogFile();
		Toast(TOAST_SAVE, "data/binary/mod folder not found");
		return;
	}
	log("Cars: mod folder exists");
	if(logFile) fprintf(logFile, "mod folder exists\n");

	char iplPath[256];
	snprintf(iplPath, sizeof(iplPath), "data/binary/ipl/*");

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(iplPath, &findData);
	if(hFind == INVALID_HANDLE_VALUE){
		log("Cars: no IPL files found");
		if(logFile) fprintf(logFile, "no IPL files found\n");
		if(logFile) fclose(logFile);
		Toast(TOAST_SAVE, "No IPL files found");
		return;
	}

	int mergedCount = 0;

	do{
		const char *filename = findData.cFileName;
		size_t namelen = strlen(filename);
		if(namelen < 4 || strcmp(filename + namelen - 4, ".ipl") != 0)
			continue;

		char iplFilepath[256];
		snprintf(iplFilepath, sizeof(iplFilepath), "data/binary/ipl/%s", filename);

		char modFilepath[256];
		snprintf(modFilepath, sizeof(modFilepath), "data/binary/mod/%s", filename);

		log("Cars: checking mod file: %s", modFilepath);
		if(logFile) fprintf(logFile, "checking mod file: %s\n", modFilepath);

		int modSize;
		uint8 *modBuf = ReadLooseFile(modFilepath, &modSize);
		if(modBuf == nil){
			log("Cars: could not read mod file: %s", modFilepath);
			if(logFile) fprintf(logFile, "could not read mod file: %s\n", modFilepath);
			continue;
		}

		CarSpawn modSpawns[1000];
		int modNumCars = ReadCarSpawnsFromBuffer(modBuf, modSize, modSpawns, 1000);
		free(modBuf);

		log("Cars: mod file has %d cars: %s", modNumCars, filename);
		if(logFile) fprintf(logFile, "mod file %s has %d cars\n", filename, modNumCars);

		if(modNumCars <= 0)
			continue;

		int iplSize;
		uint8 *iplBuf = ReadLooseFile(iplFilepath, &iplSize);
		if(iplBuf == nil)
			continue;

		int32 oldCarsOffset = *(int32*)(iplBuf + 0x3C);
		int32 oldNumCars = 0;
		CarSpawn oldSpawns[1000];
		if(iplSize >= 0x4C){
			oldNumCars = *(int32*)(iplBuf + 0x14);
			if(oldNumCars < 0) oldNumCars = 0;
			if(oldNumCars > 0 && oldCarsOffset > 0 && oldCarsOffset + oldNumCars * 48 <= iplSize){
				uint8 *oldCarsData = iplBuf + oldCarsOffset;
				for(int i = 0; i < oldNumCars && i < 1000; i++){
					uint8 *carEntry = oldCarsData + i * 48;
					oldSpawns[i].x = *(float*)(carEntry + 0);
					oldSpawns[i].y = *(float*)(carEntry + 4);
					oldSpawns[i].z = *(float*)(carEntry + 8);
					oldSpawns[i].angle = *(float*)(carEntry + 12);
					oldSpawns[i].vehicleId = *(int32*)(carEntry + 16);
					oldSpawns[i].primaryColor = *(int32*)(carEntry + 20);
					oldSpawns[i].secondaryColor = *(int32*)(carEntry + 24);
					oldSpawns[i].forceSpawn = *(int32*)(carEntry + 28);
					oldSpawns[i].alarmProb = *(int32*)(carEntry + 32);
					oldSpawns[i].lockedProb = *(int32*)(carEntry + 36);
					oldSpawns[i].unknown1 = *(int32*)(carEntry + 40);
					oldSpawns[i].unknown2 = *(int32*)(carEntry + 44);
				}
			}
		}

		int totalNumCars = gAdditiveMerge ? (oldNumCars + modNumCars) : modNumCars;

		if(logFile) fprintf(logFile, "old=%d, mod=%d, total=%d, additive=%d\n", 
			oldNumCars, modNumCars, totalNumCars, gAdditiveMerge ? 1 : 0);

		int newCarsOffset = 0x40;
		int newFileSize = newCarsOffset + totalNumCars * 48;
		if(newFileSize > iplSize){
			uint8 *newBuf = (uint8*)realloc(iplBuf, newFileSize);
			if(newBuf == nil){
				free(iplBuf);
				continue;
			}
			iplBuf = newBuf;
			iplSize = newFileSize;
		}
		*(int32*)(iplBuf + 0x3C) = newCarsOffset;

		*(int32*)(iplBuf + 0x14) = totalNumCars;
		log("Cars: writing %d cars (old=%d + mod=%d) at offset 0x%X, file size %d", 
			totalNumCars, oldNumCars, modNumCars, newCarsOffset, iplSize);
		if(logFile) fprintf(logFile, "writing %d cars (old=%d + mod=%d) at offset 0x%X, file size %d\n", 
			totalNumCars, oldNumCars, modNumCars, newCarsOffset, iplSize);

		uint8 *carsData = iplBuf + newCarsOffset;
		if(gAdditiveMerge && oldNumCars > 0){
			for(int i = 0; i < oldNumCars; i++){
				uint8 *carEntry = carsData + i * 48;
				*(float*)(carEntry + 0) = oldSpawns[i].x;
				*(float*)(carEntry + 4) = oldSpawns[i].y;
				*(float*)(carEntry + 8) = oldSpawns[i].z;
				*(float*)(carEntry + 12) = oldSpawns[i].angle;
				*(int32*)(carEntry + 16) = oldSpawns[i].vehicleId;
				*(int32*)(carEntry + 20) = oldSpawns[i].primaryColor;
				*(int32*)(carEntry + 24) = oldSpawns[i].secondaryColor;
				*(int32*)(carEntry + 28) = oldSpawns[i].forceSpawn;
				*(int32*)(carEntry + 32) = oldSpawns[i].alarmProb;
				*(int32*)(carEntry + 36) = oldSpawns[i].lockedProb;
				*(int32*)(carEntry + 40) = oldSpawns[i].unknown1;
				*(int32*)(carEntry + 44) = oldSpawns[i].unknown2;
			}
		}
		if(modNumCars > 0){
			for(int i = 0; i < modNumCars; i++){
				int dstIdx = gAdditiveMerge ? (oldNumCars + i) : i;
				uint8 *carEntry = carsData + dstIdx * 48;
				*(float*)(carEntry + 0) = modSpawns[i].x;
				*(float*)(carEntry + 4) = modSpawns[i].y;
				*(float*)(carEntry + 8) = modSpawns[i].z;
				*(float*)(carEntry + 12) = modSpawns[i].angle;
				*(int32*)(carEntry + 16) = modSpawns[i].vehicleId;
				*(int32*)(carEntry + 20) = modSpawns[i].primaryColor;
				*(int32*)(carEntry + 24) = modSpawns[i].secondaryColor;
				*(int32*)(carEntry + 28) = modSpawns[i].forceSpawn;
				*(int32*)(carEntry + 32) = modSpawns[i].alarmProb;
				*(int32*)(carEntry + 36) = modSpawns[i].lockedProb;
				*(int32*)(carEntry + 40) = modSpawns[i].unknown1;
				*(int32*)(carEntry + 44) = modSpawns[i].unknown2;
			}
		}

	FILE *f = fopen(iplFilepath, "wb");
		if(f){
			size_t written = fwrite(iplBuf, 1, iplSize, f);
			fclose(f);
			log("Cars: merged %d cars (%d old + %d mod) to %s, wrote %d bytes", totalNumCars, oldNumCars, modNumCars, filename, (int)written);
			if(logFile) fprintf(logFile, "merged %d cars (%d old + %d mod) to %s, wrote %d bytes\n", totalNumCars, oldNumCars, modNumCars, filename, (int)written);
			mergedCount++;
		}else{
			log("Cars: failed to open for writing: %s", iplFilepath);
			if(logFile) fprintf(logFile, "FAILED to open for writing: %s\n", iplFilepath);
		}

		free(iplBuf);

	}while(FindNextFileA(hFind, &findData));

	FindClose(hFind);

	if(logFile) fprintf(logFile, "mergedCount = %d\n", mergedCount);
	if(logFile) fclose(logFile);

	if(mergedCount > 0){
		log("Cars: calling Init() after merge");
		if(logFile) fprintf(logFile, "calling Init() after merge\n");
		Toast(TOAST_SAVE, "Merged car spawns from mod folder to %d IPL file(s)", mergedCount);
		Init();
		log("Cars: Init() completed, total spawns: %d", (int)carSpawns.size());
		if(logFile) fprintf(logFile, "Init() completed, total spawns: %d\n", (int)carSpawns.size());
		closeLogFile();
	}else{
		closeLogFile();
		Toast(TOAST_SAVE, "No matching IPL files found in mod folder");
	}
}

}  // namespace Cars