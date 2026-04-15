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
	ENTITY_LOCATE
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

extern float gScriptLabelDistance;
extern float gScriptCubeSize;
extern bool gRenderScriptFileName;
extern bool gRenderScriptModelName;
extern bool gRenderScriptLineNumber;

int GetNumEntities(void);
ScriptEntity* GetEntity(int index);

}

#endif
