#include "euryopa.h"
#include "cars.h"
#include <stdio.h>

static std::vector<CarSpawnPath> carSpawnPaths;

static FILE *carsLogFile = nil;

static void
CarsLog(const char *fmt, ...)
{
	if(carsLogFile == nil){
		carsLogFile = fopen("cars_debug.txt", "w");
		if(carsLogFile == nil)
			return;
	}
	va_list ap;
	va_start(ap, fmt);
	vfprintf(carsLogFile, fmt, ap);
	fflush(carsLogFile);
	va_end(ap);
}

bool Cars::gRenderCars = false;
bool Cars::gRenderAsLines = false;
bool Cars::gRenderAsCubes = true;
bool Cars::gRenderUniqueColors = false;
float Cars::gDrawDist = 300.0f;

namespace Cars {

void
Init(void)
{
}

void
LoadCarsData(void)
{
	CarsLog("Cars: Loading car spawn data (game version: %d)\n", gameversion);

	int iplCount = 0;
	int withImageIndex = 0;
	int bnryCount = 0;
	int hasCars = 0;

	for(int i = 0; i < NUMIPLS; i++){
		IplDef *ipl = GetIplDef(i);
		if(ipl == nil)
			continue;
		iplCount++;
		if(ipl->imageIndex < 0)
			continue;
		withImageIndex++;

		int size;
		uint8 *buffer = ReadFileFromImage(ipl->imageIndex, &size);
		if(buffer == nil)
			continue;

		if(*(uint32*)buffer != 0x79726E62){
			free(buffer);
			continue;
		}
		bnryCount++;

		int32 numInsts = *(int32*)(buffer + 0x04);
		int32 numCars = *(int32*)(buffer + 0x14);
		int32 carsOffset = *(int32*)(buffer + 0x3C);
		int32 expectedCarsOffset = 0x40 + (numInsts * 40);
		CarsLog("Cars: IPL %s: numInsts=%d, numCars=%d, carsOffset=0x%X, expected=0x%X, fileSize=%d\n", 
			ipl->name, numInsts, numCars, carsOffset, expectedCarsOffset, size);

		if(numCars <= 0){
			free(buffer);
			continue;
		}

		CarsLog("Cars: IPL %s cars offset = 0x%X (%d), total file size = %d\n", ipl->name, carsOffset, carsOffset, size);

		if(carsOffset <= 0 || carsOffset + numCars * 48 > size){
			CarsLog("Cars: IPL %s - invalid cars data (offset %d + %d bytes > size %d)\n", 
				ipl->name, carsOffset, numCars * 48, size);
			free(buffer);
			continue;
		}
		hasCars++;

		CarSpawnPath path;
		strncpy(path.iplName, ipl->name, sizeof(path.iplName) - 1);
		path.iplName[sizeof(path.iplName) - 1] = '\0';
		path.enabled = true;
		path.spawns.resize(numCars);

		uint8 *carsData = buffer + carsOffset;
		for(int32 j = 0; j < numCars; j++){
			uint8 *carData = carsData + j * 48;
			CarSpawnPoint &spawn = path.spawns[j];
			spawn.posX = *(float*)(carData + 0);
			spawn.posY = *(float*)(carData + 4);
			spawn.posZ = *(float*)(carData + 8);
			spawn.angle = *(float*)(carData + 12);
			spawn.carId = *(int32*)(carData + 16);
			spawn.primCol = *(int32*)(carData + 20);
			spawn.secCol = *(int32*)(carData + 24);
			spawn.forceSpawn = *(int32*)(carData + 28);
			spawn.alarm = *(int32*)(carData + 32);
			spawn.doorLock = *(int32*)(carData + 36);
			spawn.unknown1 = *(int32*)(carData + 40);
			spawn.unknown2 = *(int32*)(carData + 44);
			spawn.iplIndex = i;
		}

		carSpawnPaths.push_back(path);
		CarsLog("Cars: loaded %d car spawns from %s\n", numCars, ipl->name);
		free(buffer);
	}

	CarsLog("Cars: loaded %d IPLs with car spawns (total ipl=%d, imgidx=%d, bnry=%d)\n", hasCars, iplCount, withImageIndex, bnryCount);
}

void
Render(void)
{
	if(!Cars::gRenderCars)
		return;

	if(carSpawnPaths.empty()){
		LoadCarsData();
		if(carSpawnPaths.empty())
			return;
	}

	uint8 alpha = (uint8)(gCollisionWireframeAlpha * 255.0f);
	rw::RGBA defaultCol = { 255, 165, 0, alpha };

	for(size_t i = 0; i < carSpawnPaths.size(); i++){
		CarSpawnPath &path = carSpawnPaths[i];
		if(path.spawns.empty() || !path.enabled)
			continue;

		rw::RGBA col = defaultCol;
		if(Cars::gRenderUniqueColors){
			uint hue = (i * 59) % 360;
			float h = hue / 60.0f;
			float c = 1.0f;
			float x = c * (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));
			float m = 0.0f;
			float r, g, b;
			if(h < 1.0f){ r = c; g = x; b = m; }
			else if(h < 2.0f){ r = x; g = c; b = m; }
			else if(h < 3.0f){ r = m; g = c; b = x; }
			else if(h < 4.0f){ r = m; g = x; b = c; }
			else if(h < 5.0f){ r = x; g = m; b = c; }
			else{ r = c; g = m; b = x; }
			col = { (uint8)(r * 255), (uint8)(g * 255), (uint8)(b * 255), alpha };
		}

		for(size_t j = 0; j < path.spawns.size(); j++){
			CarSpawnPoint &spawn = path.spawns[j];
			rw::V3d pos = { spawn.posX, spawn.posY, spawn.posZ };

			if(TheCamera.distanceTo(pos) > Cars::gDrawDist)
				continue;

			if(Cars::gRenderAsLines && j > 0){
				CarSpawnPoint &prev = path.spawns[j-1];
				rw::V3d prevPos = { prev.posX, prev.posY, prev.posZ };
				RenderLine(prevPos, pos, col, col);
			}

			if(Cars::gRenderAsCubes){
				float half = 0.5f;
				rw::V3d v[8] = {
					{ pos.x - half, pos.y - half, pos.z - half },
					{ pos.x + half, pos.y - half, pos.z - half },
					{ pos.x - half, pos.y + half, pos.z - half },
					{ pos.x + half, pos.y + half, pos.z - half },
					{ pos.x - half, pos.y - half, pos.z + half },
					{ pos.x + half, pos.y - half, pos.z + half },
					{ pos.x - half, pos.y + half, pos.z + half },
					{ pos.x + half, pos.y + half, pos.z + half }
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
	}
}

int
GetNumPaths(void)
{
	return (int)carSpawnPaths.size();
}

CarSpawnPath *
GetPath(int index)
{
	if(index >= 0 && index < (int)carSpawnPaths.size())
		return &carSpawnPaths[index];
	return nil;
}

void
SetAllPaths(bool enabled)
{
	for(size_t i = 0; i < carSpawnPaths.size(); i++)
		carSpawnPaths[i].enabled = enabled;
}

}  // namespace Cars