#ifndef CARREC_H
#define CARREC_H

#include "euryopa.h"
#include <vector>

struct CarrecPoint {
	int32 time;
	int16 velX, velY, velZ;
	int8 rightX, rightY, rightZ;
	int8 topX, topY, topZ;
	int8 steering;
	int8 gas;
	int8 brake;
	int8 handbrake;
	float posX, posY, posZ;
};

struct CarrecRecording {
	int id;
	std::vector<CarrecPoint> points;
};

namespace Carrec {
	extern bool gRenderCarrec;
	extern float gCarrecDrawDist;
	extern int gSelectedCarrec;
	extern int gCarrecAnimationTime;
	extern bool gCarrecAnimate;

	void Init(const char *dataPath);
	void Shutdown(void);
	int GetNumRecordings(void);
	CarrecRecording *GetRecording(int idx);
	void Render(void);
}

#endif
