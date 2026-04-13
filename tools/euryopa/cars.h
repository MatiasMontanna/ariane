#ifndef CARS_H
#define CARS_H

#include <vector>

struct CarSpawn {
	char iplName[32];
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
};

namespace Cars {
void Init(void);
void Render(void);
void ExportCSV(void);
void RenderPicking(void);
int PickCarSpawn(void);
void SelectCarSpawn(int index);
int GetSelectedCarSpawnIndex(void);
CarSpawn* GetSelectedCarSpawn(void);
void MoveSelectedCarSpawn(float x, float y, float z);
void RotateSelectedCarSpawn(float angle);
void SetSelectedCarSpawnProperty(int property, int value);
void SaveAllCarSpawns(void);

extern bool gRenderAsCubes;
extern bool gRenderVehicleId;
extern bool gRenderPrimaryColor;
extern bool gRenderSecondaryColor;
extern bool gRenderForceSpawn;
extern bool gRenderAlarmProb;
extern bool gRenderLockedProb;
extern bool gRenderUnknown1;
extern bool gRenderUnknown2;
extern bool gRenderFileName;
extern bool gRenderAngle;
extern bool gReplaceWithModCars;
extern bool gAdditiveMerge;
bool HasCarSpawns(void);
int GetNumCarSpawns(void);
CarSpawn* GetCarSpawn(int index);
void MergeModCarSpawns(void);
}

#endif