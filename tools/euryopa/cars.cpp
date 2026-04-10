#include "euryopa.h"
#include "cars.h"
#include <vector>

namespace Cars {

static std::vector<CarSpawn> carSpawns;
static bool loaded = false;

void Init(void)
{
	if(loaded)
		return;

	carSpawns.clear();

	for(int slot = 0; slot < NUMIPL; slot++){
		IplDef *ipl = GetIplDef(slot);
		if(ipl->imageIndex < 0)
			continue;

		int32 imageIndex = ipl->imageIndex;
		int img = imageIndex>>24 & 0xFF;
		int entry = imageIndex & 0xFFFFFF;

		if(img < 0 || img >= numCdImages)
			continue;
		if(entry < 0 || entry >= cdImages[img].directorySize)
			continue;

		int size = 0;
		uint8 *buffer = ReadFileFromImage(imageIndex, &size);
		if(!buffer || size < 0x4C)
			continue;

		if(*(uint32*)buffer != 0x79726E62)
			continue;

		int32 numCars = *(int32*)(buffer + 0x14);
		int32 carsOffset = *(int32*)(buffer + 0x3C);

		if(numCars <= 0 || carsOffset <= 0)
			continue;

		if(carsOffset + numCars * 48 > size)
			continue;

		uint8 *carsData = buffer + carsOffset;
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
			spawn.iplName = ipl->name;

			carSpawns.push_back(spawn);
		}

		free(buffer);
	}

	loaded = true;
}

void Render(void)
{
	if(!loaded)
		return;

	for(const auto& car : carSpawns){
		DrawWireBox(car.x - 1.5f, car.y - 1.5f, car.z,
		            car.x + 1.5f, car.y + 1.5f, car.z + 3.0f,
		            0xFF00FF00);
	}
}

void Shutdown(void)
{
	carSpawns.clear();
	loaded = false;
}

}