#include "euryopa.h"
#include "cars.h"
#include "modloader.h"

static std::vector<CarSpawn> carSpawns;

bool Cars::gRenderAsCubes = true;

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
	}
}

}  // namespace Cars