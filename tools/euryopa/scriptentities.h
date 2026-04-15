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
	ENTITY_COORD
};

struct ScriptEntity {
	ScriptEntityType type;
	float x, y, z;
	float heading;
	int modelId;
	char modelName[32];
	char scriptName[64];
	char varName[64];
	int lineNum;
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
extern bool gRenderScriptCoords;

int GetNumEntities(void);
ScriptEntity* GetEntity(int index);

}

#endif
