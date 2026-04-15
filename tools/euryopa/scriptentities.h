#ifndef SCRIPTENTITIES_H
#define SCRIPTENTITIES_H

#include <vector>
#include <string>

enum ScriptEntityType {
	ENTITY_CAR,
	ENTITY_PED,
	ENTITY_OBJECT,
	ENTITY_PICKUP,
	ENTITY_BLIP,
	ENTITY_FX,
	ENTITY_CHECKPOINT,
	ENTITY_GENERATOR,
	ENTITY_LOCATE,
	ENTITY_CAMERA,
	ENTITY_ROUTE,
	ENTITY_TELEPORT,
	ENTITY_PLAYER,
	ENTITY_GANG_CAR,
	ENTITY_GARAGE,
	ENTITY_PATH_POINT,
	ENTITY_FIRE,
	ENTITY_EXPLOSION,
	ENTITY_SEARCHLIGHT,
	ENTITY_CORONA,
	ENTITY_MARKER,
	ENTITY_DOOR,
	ENTITY_REMOTE_CAR,
	ENTITY_TRAIN,
	ENTITY_BOAT,
	ENTITY_HELI,
	ENTITY_WEATHER,
	ENTITY_ZONE,
	ENTITY_SPAWN,
	ENTITY_PROJECTILE
};

struct ScriptEntity {
	ScriptEntityType type;
	float x, y, z;
	float heading;
	float radius;
	int modelId;
	int lineNum;
	char modelName[32];
	char scriptName[64];
	char varName[64];
};

namespace ScriptEntities {

void Init(void);
void Render(void);
void Shutdown(void);

extern bool gRenderScriptEntities;
extern bool gRenderScriptCars;
extern bool gRenderScriptPeds;
extern bool gRenderScriptObjects;
extern bool gRenderScriptPickups;
extern bool gRenderScriptBlips;
extern bool gRenderScriptFx;
extern bool gRenderScriptCheckpoints;
extern bool gRenderScriptGenerators;
extern bool gRenderScriptLocates;
extern bool gRenderScriptCameras;
extern bool gRenderScriptRoutes;
extern bool gRenderScriptTeleports;
extern bool gRenderScriptPlayers;
extern bool gRenderScriptGangCars;
extern bool gRenderScriptGarages;
extern bool gRenderScriptPathPoints;
extern bool gRenderScriptFires;
extern bool gRenderScriptExplosions;
extern bool gRenderScriptSearchlights;
extern bool gRenderScriptCoronas;
extern bool gRenderScriptMarkers;
extern bool gRenderScriptDoors;
extern bool gRenderScriptRemoteCars;
extern bool gRenderScriptTrains;
extern bool gRenderScriptBoats;
extern bool gRenderScriptHelis;
extern bool gRenderScriptWeathers;
extern bool gRenderScriptZones;
extern bool gRenderScriptSpawns;
extern bool gRenderScriptProjectiles;

extern float gScriptLabelDistance;
extern float gScriptCubeDistance;
extern float gScriptCubeSize;
extern bool gRenderScriptText;
extern bool gRenderScriptFileName;
extern bool gRenderScriptModelName;
extern bool gRenderScriptLineNumber;
extern int gMaxScriptLabels;

int GetNumEntities(void);
ScriptEntity* GetEntity(int index);

}

#endif
