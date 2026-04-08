#ifndef CARREC_H
#define CARREC_H

#include <vector>

struct CarrecNode {
	float time;
	float velocityX, velocityY, velocityZ;
	int16 orientW, orientX, orientY, orientZ;
	int16 steering;
	uint16 gas;
	uint16 brake;
	float posX, posY, posZ;
};

struct CarrecPath {
	char name[24];
	std::vector<CarrecNode> nodes;
};

void CarrecInit(void);
void CarrecRender(void);

#endif