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
	bool enabled;
};

namespace Carrec {
void Init(void);
void Render(void);
extern bool gRenderAsLines;
extern bool gRenderAsCubes;
extern bool gRenderPosition;
extern bool gRenderVelocity;
extern bool gRenderTime;
extern bool gRenderSteering;
extern bool gRenderLastNode;
extern bool gRenderUniqueColors;
extern bool gRenderLabels;
extern bool gRenderTextVelocity;
extern bool gRenderTextTime;
extern bool gRenderTextSteering;
extern bool gRenderTextGas;
extern bool gRenderTextBrake;
extern bool gRenderTextHandbrake;
int GetNumPaths(void);
CarrecPath *GetPath(int index);
void SetAllPaths(bool enabled);
}

#endif