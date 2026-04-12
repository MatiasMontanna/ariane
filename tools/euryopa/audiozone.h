#ifndef AUDIOZONE_H
#define AUDIOZONE_H

#include <vector>

struct AudioZoneEntry {
	char name[32];
	int id;
	int switchVal;
	float x1, y1, z1;
	float x2, y2, z2;
	int type;
};

namespace AudioZones {
void Init(void);
void Render(void);
bool HasAudioZones(void);
extern bool gRenderAudioZones;
}

#endif