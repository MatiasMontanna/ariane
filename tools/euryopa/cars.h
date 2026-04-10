#ifndef CARS_H
#define CARS_H

#include "euryopa.h"

namespace Cars {

struct CarSpawn {
    float x, y, z;
    float angle;
    int32 vehicleId;
    int32 primaryColor;
    int32 secondaryColor;
    int32 forceSpawn;
    int32 alarmProb;
    int32 lockedProb;
    int32 unknown1;
    int32 unknown2;
    const char* iplName;
};

void Init(void);
void Render(void);
void Shutdown(void);

}

#endif