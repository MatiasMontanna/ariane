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
	ENTITY_PROJECTILE,
	ENTITY_AUDIO,
	ENTITY_DRAW,
	ENTITY_TASK,
	ENTITY_DAMAGE,
	ENTITY_MISSION
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
	char comment[128];
};

struct ScriptFile {
	char filename[128];
	char fullPath[512];
	bool enabled;
	int numEntities;
	std::vector<int> entityIndices;
};

namespace ScriptEntities {

void Init(void);
void Render(void);
void Shutdown(void);
void Reload(void);
void TeleportToEntity(int entityIndex);
void TeleportToCoords(float x, float y, float z);

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
extern bool gRenderScriptAudio;
extern bool gRenderScriptDraw;
extern bool gRenderScriptTasks;
extern bool gRenderScriptDamage;
extern bool gRenderScriptMission;

extern float gScriptLabelDistance;
extern float gScriptCubeDistance;
extern float gScriptCubeSize;
extern bool gRenderScriptText;
extern bool gRenderScriptFileName;
extern bool gRenderScriptModelName;
extern bool gRenderScriptLineNumber;
extern bool gRenderScriptComment;
extern int gMaxScriptLabels;

extern bool gSelectScriptEntities;
extern int gSelectedScriptEntity;
extern bool gScriptEntityGizmoEnabled;
extern int gScriptEntityGizmoMode;

void SelectScriptEntity(int index);
void DeselectScriptEntity(void);
int PickScriptEntity(float mouseX, float mouseY);
void HandleScriptEntityGizmo(float* viewMatrix, float* projMatrix);
void RenderScriptEntityProperties(void);

int GetNumEntities(void);
int GetNumFiles(void);
ScriptEntity* GetEntity(int index);
ScriptFile* GetFile(int index);
ScriptEntity* GetEntityByFileAndIndex(int fileIndex, int entityInFileIndex);
void SetFileEnabled(int fileIndex, bool enabled);
bool IsFileEnabled(int fileIndex);

const char* GetEntityTypeName(ScriptEntityType type);

}

namespace Holes {
	struct Hole {
		float x, y, z;
		float size;
		int type;
		char name[64];
	};

	extern bool gRenderHoles;
	extern float gHoleDrawDist;
	extern float gHoleCubeSize;
	extern float gHoleZOffset;

	void Init(void);
	void Shutdown(void);
	void Reload(void);
	int GetNumHoles(void);
	Hole* GetHole(int idx);
	void Render(void);
}

#endif
