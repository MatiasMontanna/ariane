#ifndef CARREC_H
#define CARREC_H

#include "euryopa.h"

struct CarrecPoint {
	int32 time;
	int16 velX, velY, velZ;
	int8 rightX, rightY, rightZ;
	int8 topX, topY, topZ;
	int8 steering;
	int8 gas;
	int8 brake;
	int8 handbrake;
	float posX, posY, posZ;
};

struct CarrecPath {
	std::string name;
	std::vector<CarrecPoint> points;
};

namespace Carrec {

void Load(void);
void Render(void);
int GetNumPaths(void);
const char *GetPathName(int i);

}

#endif