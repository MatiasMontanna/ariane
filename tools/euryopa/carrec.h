#ifndef _CARREC_H
#define _CARREC_H

#include "euryopa.h"

struct CarrecNode
{
	int32 time;
	int16 velocityX;
	int16 velocityY;
	int16 velocityZ;
	int8 rightX;
	int8 rightY;
	int8 rightZ;
	int8 topX;
	int8 topY;
	int8 topZ;
	int8 steeringAngle;
	int8 gasPedal;
	int8 brakePedal;
	int8 handbrake;
	float posX;
	float posY;
	float posZ;
};

struct CarrecPath
{
	char name[24];
	CarrecNode *nodes;
	int numNodes;
};

namespace Carrec {

void Init(void);
void Shutdown(void);
void Render(void);
int GetNumPaths(void);
const char *GetPathName(int idx);

}

#endif