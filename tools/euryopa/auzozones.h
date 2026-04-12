#ifndef AUZOZONES_H
#define AUZOZONES_H

#include <rw.h>

namespace AuzoZones
{

struct AuzoZone
{
	char name[32];
	int soundId;
	int switchId;
	enum Type { BOX, SPHERE } type;
	union {
		struct { float x1, y1, z1, x2, y2, z2; } box;
		struct { float x, y, z, radius; } sphere;
	};
};

void LoadAllAuzoZones(void);
void Render(void);
void Clear(void);

extern bool gRenderAuzoZones;

}

#endif