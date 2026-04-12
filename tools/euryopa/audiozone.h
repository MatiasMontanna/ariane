#ifndef AUDIOZONE_H
#define AUDIOZONE_H

struct AudioZoneEntry {
	char name[32];
	int id;
	int switchVal;
	struct CBox {
		rw::V3d min;
		rw::V3d max;
		void FindMinMax(void);
	} box;
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