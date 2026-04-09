#ifndef CARS_H
#define CARS_H

#include <vector>

struct CarSpawnPoint {
	float posX, posY, posZ;
	float angle;
	int32 carId;
	int32 primCol;
	int32 secCol;
	int32 forceSpawn;
	int32 alarm;
	int32 doorLock;
	int32 unknown1;
	int32 unknown2;
	int32 iplIndex;
};

struct CarSpawnPath {
	char iplName[32];
	std::vector<CarSpawnPoint> spawns;
	bool enabled;
};

namespace Cars {
void Init(void);
void Render(void);
extern bool gRenderCars;
extern bool gRenderAsLines;
extern bool gRenderAsCubes;
extern bool gRenderUniqueColors;
extern float gDrawDist;
int GetNumPaths(void);
CarSpawnPath *GetPath(int index);
void SetAllPaths(bool enabled);
}

#endif