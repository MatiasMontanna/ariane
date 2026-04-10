#include "euryopa.h"
#include "cars.h"
#include "cdimage.h"
#include <vector>

namespace Cars {

struct CarSpawn {
	float x, y, z;
	float angle;
	int32 vehicleId;
	int32 primaryColor;
	int32 secondaryColor;
	int32 forceSpawn;
	int32 alarmProb;
	int32 lockedProb;
};

static std::vector<CarSpawn> carSpawns;
static bool loaded = false;

void Init(void)
{
	if(loaded)
		return;

	carSpawns.clear();

	CdImage *gta3img = &cdImages[0];

	for(int i = 0; i < gta3img->directorySize; i++){
		DirEntry *de = &gta3img->directory[i];
		if(de->filetype != FILE_IPL)
			continue;

		int size = 0;
		uint8 *buffer = ReadFileFromImage(i, &size);
		if(!buffer || size < 0x4C)
			continue;

		if(*(uint32*)buffer != 0x79726E62)
			continue;

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

			carSpawns.push_back(spawn);
		}

		free(buffer);
	}

	log("Cars: found %d car spawns in gta3.img\n", carSpawns.size());
	loaded = true;
}

void Render(void)
{
	if(!loaded || carSpawns.empty())
		return;

	for(const auto& car : carSpawns){
		DrawWireBox(car.x - 1.5f, car.y - 1.5f, car.z,
		            car.x + 1.5f, car.y + 1.5f, car.z + 3.0f,
		            0xFF00FF00);
	}
}

}