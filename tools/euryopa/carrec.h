#ifndef CARREC_H
#define CARREC_H

#include <vector>

struct CarrecNode {
	int32 time;
	int16 velocityX, velocityY, velocityZ;
	int8 orientRight[3];
	int8 orientTop[3];
	int8 steering;
	int8 gas;
	int8 brake;
	int8 handbrake;
	float posX, posY, posZ;
};

struct CarrecPath {
	char name[24];
	std::vector<CarrecNode> nodes;
};

namespace Carrec {
void Init(void);
void Render(void);
extern bool gRenderAsLines;
extern bool gRenderAsCubes;
}

#endif