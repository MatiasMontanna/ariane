#ifndef CARS_H
#define CARS_H

#include <vector>

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
};

namespace Cars {

void Init(void);
void Render(void);
extern bool gRenderAsCubes;

}

#endif