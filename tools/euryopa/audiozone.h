#ifndef AUDIOZONE_H
#define AUDIOZONE_H

#include "euryopa.h"

struct AudioZoneEntry {
	char name[32];
	int id;
	int switchVal;
	// Type 1: box
	CBox box;
	// Type 2: sphere
	rw::V3d center;
	float radius;
	int type;
};

namespace AudioZones {
void Init(void);
void Render(void);
bool HasAudioZones(void);
extern bool gRenderAudioZones;
}

#endif