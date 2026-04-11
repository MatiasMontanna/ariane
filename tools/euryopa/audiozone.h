#ifndef AUDIOZONE_H
#define AUDIOZONE_H

#include "euryopa.h"

struct AudioZoneEntry
{
	char name[32];
	int id;
	int switchVal;
	CBox box;
	rw::V3d center;
	float radius;
	int type;
};

namespace AudioZones {

extern bool gRenderAudioZones;

void Init(void);
bool HasAudioZones(void);
void Render(void);

}

#endif