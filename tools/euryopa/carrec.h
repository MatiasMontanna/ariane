#ifndef CARREC_H
#define CARREC_H

#include <vector>

struct CarrecNode {
	int32 time;
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