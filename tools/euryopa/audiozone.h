#ifndef AUDIOZONE_H
#define AUDIOZONE_H

struct AudioZoneEntry {
	char name[32];
	int id;
	int switchVal;
	float boxMinX, boxMinY, boxMinZ;
	float boxMaxX, boxMaxY, boxMaxZ;
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