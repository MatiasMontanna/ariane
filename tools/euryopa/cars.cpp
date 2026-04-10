#include "euryopa.h"
#include "cars.h"
#include "cdimage.h"

struct CarSpawn {
	char iplName[32];
	float x, y, z;
	float angle;
	int32 vehicleId;
	int32 primaryColor;
	int32 secondaryColor;
	int32 forceSpawn;
	int32 alarmProb;
	int32 lockedProb;
	int32 unknown1;
	int32 unknown2;
};

static std::vector<CarSpawn> carSpawns;

bool Cars::gRenderAsCubes = true;

namespace Cars {

void
Init(void)
{
	log("Cars: Starting...\n");

	if(!cdImages){
		log("Cars: cdImages not initialized\n");
		return;
	}

	CdImage *gta3img = &cdImages[0];
	if(!gta3img || !gta3img->directory){
		log("Cars: gta3.img not loaded\n");
		return;
	}

	log("Cars: scanning gta3.img (%d entries)\n", gta3img->directorySize);

	for(int i = 0; i < gta3img->directorySize; i++){
		DirEntry *de = &gta3img->directory[i];
		if(de->filetype != FILE_IPL)
			continue;

		int size = 0;
		uint8 *buffer = ReadFileFromImage(i, &size);
		if(!buffer || size < 0x4C){
			if(buffer) free(buffer);
			continue;
		}

		uint32 magic = *(uint32*)buffer;
		if(magic != 0x79726E62){
			free(buffer);
			continue;
		}

		int32 numCars = *(int32*)(buffer + 0x10);
		if(numCars <= 0){
			free(buffer);
			continue;
		}

		int32 carsOffset = *(int32*)(buffer + 0x38);
		if(carsOffset <= 0 || carsOffset + numCars * 48 > size){
			free(buffer);
			continue;
		}

		log("Cars: %s has %d cars\n", de->name, numCars);

		uint8 *carsData = buffer + carsOffset;
		for(int j = 0; j < numCars; j++){
			uint8 *carEntry = carsData + j * 48;

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
			strncpy(spawn.iplName, de->name, sizeof(spawn.iplName) - 1);
			spawn.iplName[sizeof(spawn.iplName) - 1] = '\0';

			carSpawns.push_back(spawn);
		}

		free(buffer);
	}

	log("Cars: loaded %d spawns\n", (int)carSpawns.size());
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