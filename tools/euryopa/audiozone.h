#ifndef AUDIOZONE_H
#define AUDIOZONE_H

#include "euryopa.h"

namespace AudioZones
{

struct AudioZoneEntry
{
	char name[32];
	int id;
	int switchVal;
	// Type 1: box
	CBox box;
	// Type 2: sphere
	rw::V3d center;
	float radius;
	int type; // 1 = box, 2 = sphere
};

void LoadIplAudioZones(const char *path);
void Render(void);

}

#endif