#include "euryopa.h"
#include "scriptentities.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

std::vector<ScriptEntity> gEntities;

bool ScriptEntities::gRenderScriptEntities = false;
bool ScriptEntities::gRenderScriptCars = true;
bool ScriptEntities::gRenderScriptPeds = true;
bool ScriptEntities::gRenderScriptObjects = true;
bool ScriptEntities::gRenderScriptPickups = true;
bool ScriptEntities::gRenderScriptBlips = true;
bool ScriptEntities::gRenderScriptFx = true;
bool ScriptEntities::gRenderScriptCheckpoints = true;
bool ScriptEntities::gRenderScriptGenerators = true;
bool ScriptEntities::gRenderScriptLocates = true;
bool ScriptEntities::gRenderScriptCameras = true;
bool ScriptEntities::gRenderScriptRoutes = true;
bool ScriptEntities::gRenderScriptTeleports = true;

float ScriptEntities::gScriptLabelDistance = 200.0f;
float ScriptEntities::gScriptCubeSize = 0.5f;
bool ScriptEntities::gRenderScriptFileName = true;
bool ScriptEntities::gRenderScriptModelName = true;
bool ScriptEntities::gRenderScriptLineNumber = false;

static bool isWhitespace(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool isNumber(char c) {
	return (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '+';
}

static void skipWhitespace(const char*& p) {
	while (*p && isWhitespace(*p)) p++;
}

static void skipToWhitespace(const char*& p) {
	while (*p && !isWhitespace(*p)) p++;
}

static std::string getNextWord(const char*& p) {
	skipWhitespace(p);
	const char* start = p;
	while (*p && !isWhitespace(*p)) p++;
	return std::string(start, p - start);
}

static std::string getLine(const char*& p) {
	const char* start = p;
	while (*p && *p != '\n' && *p != '\r') p++;
	std::string line(start, p - start);
	if (*p == '\r') p++;
	if (*p == '\n') p++;
	return line;
}

static bool tryParseFloat(const char*& p, float& out) {
	skipWhitespace(p);
	if (!*p || !isNumber(*p)) return false;
	
	char* end;
	double val = strtod(p, &end);
	if (end == p) return false;
	p = end;
	out = (float)val;
	return true;
}

static bool tryParseInt(const char*& p, int& out) {
	skipWhitespace(p);
	if (!*p || !isNumber(*p)) return false;
	
	char* end;
	long val = strtol(p, &end, 10);
	if (end == p) return false;
	p = end;
	out = (int)val;
	return true;
}

static void addEntity(std::vector<ScriptEntity>& ents, ScriptEntityType type, float x, float y, float z, 
					 const char* modelName, const char* scriptName, const char* varName, int lineNum, float heading = 0.0f, float radius = 0.0f) {
	ScriptEntity e;
	e.type = type;
	e.x = x;
	e.y = y;
	e.z = z;
	e.heading = heading;
	e.radius = radius;
	e.modelId = 0;
	e.lineNum = lineNum;
	if (modelName) {
		strncpy(e.modelName, modelName, sizeof(e.modelName) - 1);
		e.modelName[sizeof(e.modelName) - 1] = '\0';
	} else {
		e.modelName[0] = '\0';
	}
	if (scriptName) {
		strncpy(e.scriptName, scriptName, sizeof(e.scriptName) - 1);
		e.scriptName[sizeof(e.scriptName) - 1] = '\0';
	} else {
		e.scriptName[0] = '\0';
	}
	if (varName) {
		strncpy(e.varName, varName, sizeof(e.varName) - 1);
		e.varName[sizeof(e.varName) - 1] = '\0';
	} else {
		e.varName[0] = '\0';
	}
	ents.push_back(e);
}

void
ScriptEntities::Init(void)
{
	gEntities.clear();
	log("ScriptEntities: Init - parsing scripts folder");

	std::map<std::string, float> coordVars;

	char exeDir[256];
	if (!GetEditorRootDirectory(exeDir, sizeof(exeDir))) {
		log("ScriptEntities: failed to get executable directory\n");
		return;
	}

	char scriptsDir[512];
	snprintf(scriptsDir, sizeof(scriptsDir), "%s/scripts", exeDir);

	WIN32_FIND_DATAA findData;
	HANDLE hFind;

	char searchPath[512];
	char baseDir[512];
	strncpy(baseDir, scriptsDir, sizeof(baseDir) - 1);
	baseDir[sizeof(baseDir) - 1] = '\0';

	char dirsToSearch[64][512];
	int numDirs = 1;
	strncpy(dirsToSearch[0], scriptsDir, 511);
	dirsToSearch[0][511] = '\0';

	int processed = 0;
	while (processed < numDirs && numDirs < 64) {
		snprintf(searchPath, sizeof(searchPath), "%s/*", dirsToSearch[processed]);

		hFind = FindFirstFileA(searchPath, &findData);
		if (hFind == INVALID_HANDLE_VALUE) {
			processed++;
			continue;
		}

		do {
			const char* name = findData.cFileName;
			if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
				continue;

			char fullPath[512];
			snprintf(fullPath, sizeof(fullPath), "%s/%s", dirsToSearch[processed], name);

			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if (numDirs < 64) {
					strncpy(dirsToSearch[numDirs], fullPath, 511);
					dirsToSearch[numDirs][511] = '\0';
					numDirs++;
				}
			} else {
				size_t namelen = strlen(name);
				if (namelen >= 3 && strcmp(name + namelen - 3, ".sc") == 0) {
					parseScFile(fullPath, baseDir, name, coordVars);
				}
			}
		} while (FindNextFileA(hFind, &findData));

		FindClose(hFind);
		processed++;
	}

	log("ScriptEntities: loaded %d entities\n", (int)gEntities.size());
}

static void
parseScFile(const char* filepath, const char* baseDir, const char* filename, std::map<std::string, float>& coordVars)
{
	FILE* f = fopen(filepath, "r");
	if (!f) return;

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* buffer = (char*)malloc(fsize + 1);
	fread(buffer, 1, fsize, f);
	buffer[fsize] = '\0';
	fclose(f);

	const char* lastSlash = strrchr(filepath, '/');
	const char* lastBackslash = strrchr(filepath, '\\');
	if (!lastSlash) lastSlash = filepath - 1;
	if (!lastBackslash) lastBackslash = filepath - 1;
	if (lastBackslash > lastSlash) lastSlash = lastBackslash;

	const char* relPathStart = strstr(filepath, "scripts/");
	if (!relPathStart) relPathStart = strstr(filepath, "scripts\\");
	if (relPathStart) relPathStart += 8;
	else relPathStart = lastSlash + 1;

	char relPath[512];
	strncpy(relPath, relPathStart, sizeof(relPath) - 1);
	relPath[sizeof(relPath) - 1] = '\0';

	const char* dotPos = strrchr(relPath, '.');
	if (dotPos) {
		size_t nameLen = dotPos - relPath;
		if (nameLen < sizeof(relPath)) relPath[nameLen] = '\0';
	}

	coordVars.clear();

		std::string scriptName = relPath;
		const char* p = buffer;
		int lineNum = 0;
		std::string currentLine;

		while (*p) {
			lineNum++;
			currentLine = getLine(p);
			const char* line = currentLine.c_str();
			const char* lp = line;

			skipWhitespace(lp);
			if (!*lp || *lp == '/' || *lp == '*') continue;

			std::string cmd = getNextWord(lp);

			if (cmd == "SCRIPT_NAME") {
				std::string name = getNextWord(lp);
				scriptName = name;
				continue;
			}

			if (cmd == "VAR_FLOAT" || cmd == "LVAR_FLOAT") {
				while (*lp) {
					skipWhitespace(lp);
					if (!*lp || *lp == '/' || *lp == '*') break;
					std::string var = getNextWord(lp);
					if (var.length() > 0 && var[var.length()-1] == ',') {
						var = var.substr(0, var.length()-1);
					}
					if (var.length() > 0) {
						coordVars[var] = 0.0f;
					}
					while (*lp && *lp != ',' && *lp != '\n') lp++;
					if (*lp == ',') lp++;
				}
				continue;
			}

			if (cmd == "CREATE_CAR" || cmd == "CREATE_CAR_" || cmd == "CREATE_PARKED_CAR") {
				skipWhitespace(lp);
				std::string model = getNextWord(lp);
				
				float x = 0, y = 0, z = 0, h = 0;
				if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
					tryParseFloat(lp, h);
					addEntity(gEntities, ENTITY_CAR, x, y, z, model.c_str(), scriptName.c_str(), "", lineNum, h);
				}
				continue;
			}

			if (cmd == "CREATE_CHAR" || cmd == "CREATE_CHAR_PED" || cmd == "CREATE_CHAR_INSIDE_CAR" ||
				cmd == "CREATE_CHAR_AS_PASSENGER" || cmd == "CREATE_CHAR_AS_DRIVER") {
				skipWhitespace(lp);
				skipToWhitespace(lp);
				skipWhitespace(lp);
				std::string model = getNextWord(lp);
				
				float x = 0, y = 0, z = 0;
				if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
					addEntity(gEntities, ENTITY_PED, x, y, z, model.c_str(), scriptName.c_str(), "", lineNum);
				}
				continue;
			}

			if (cmd == "CREATE_OBJECT" || cmd == "CREATE_OBJECT_NO_ROTATE" ||
				cmd == "CREATE_OBJECT_INTSA" || cmd == "CREATE_RADAR_MARKER") {
				skipWhitespace(lp);
				std::string model = getNextWord(lp);
				
				float x = 0, y = 0, z = 0;
				if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
					addEntity(gEntities, ENTITY_OBJECT, x, y, z, model.c_str(), scriptName.c_str(), "", lineNum);
				}
				continue;
			}

			if (cmd == "CREATE_PICKUP" || cmd == "CREATE_PICKUP_MONEY" ||
				cmd == "CREATE_AMMO_PICKUP" || cmd == "CREATE_WEAPON_PICKUP" ||
				cmd == "CREATE_FLOATING_BLIP_2D" || cmd == "CREATE_PICKUP_WITH_ANGLE") {
				skipWhitespace(lp);
				std::string model = getNextWord(lp);
				
				float x = 0, y = 0, z = 0;
				if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
					addEntity(gEntities, ENTITY_PICKUP, x, y, z, model.c_str(), scriptName.c_str(), "", lineNum);
				}
				continue;
			}

			if (cmd == "ADD_BLIP_FOR_COORD" || cmd == "ADD_BLIP_FOR_COORD_2" ||
				cmd == "ADD_SHORT_RANGE_BLIP_FOR_COORD" || cmd == "ADD_MISSION_BLIP_FOR_COORD" ||
				cmd == "ADD_SCRIPTED_SPRITE" || cmd == "SET_COORD_BLIP_TEMP") {
				float x = 0, y = 0, z = 0;
				if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
					addEntity(gEntities, ENTITY_BLIP, x, y, z, "", scriptName.c_str(), "", lineNum);
				}
				continue;
			}

			if (cmd == "CREATE_FX_SYSTEM" || cmd == "CREATE_FX_SYSTEM_PP" || 
				cmd == "CREATE_FX_SYSTEM_W ember" || cmd == "CREATE_EXPLOSION") {
				std::string fxName = getNextWord(lp);
				float x = 0, y = 0, z = 0;
				if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
					addEntity(gEntities, ENTITY_FX, x, y, z, fxName.c_str(), scriptName.c_str(), "", lineNum);
				}
				continue;
			}

			if (cmd == "SWITCH_CAR_GENERATOR") {
				float x = 0, y = 0, z = 0;
				std::string dummy;
				if (tryParseFloat(lp, x)) {
					y = x;
					z = x;
					addEntity(gEntities, ENTITY_GENERATOR, x, y, z, "", scriptName.c_str(), "", lineNum);
				}
				continue;
			}

			if (cmd == "LOCATE_CHAR_ANY_MEANS_3D" || cmd == "LOCATE_CHAR_ANY_MEANS_2D" ||
				cmd == "LOCATE_CHAR_ON_FOOT_3D" || cmd == "LOCATE_CHAR_ON_FOOT_2D" ||
				cmd == "LOCATE_CHAR_IN_CAR_3D" || cmd == "LOCATE_CHAR_IN_CAR_2D" ||
				cmd == "LOCATE_STOPPED_CHAR_ANY_MEANS_3D" || cmd == "LOCATE_STOPPED_CHAR_ANY_MEANS_2D" ||
				cmd == "LOCATE_CAR_3D" || cmd == "LOCATE_CAR_2D" ||
				cmd == "LOCATE_CHAR_ANY_MEANS_CAR_2D" || cmd == "LOCATE_CHAR_ANY_MEANS_CAR_3D" ||
				cmd == "LOCATE_CHAR_IN_CAR_3D" || cmd == "LOCATE_CHAR_ON_FOOT_3D" ||
				cmd == "LOCATE_CHAR_ON_FOOT_2D" || cmd == "LOCATE_CHAR_STANDING" ||
				cmd == "LOCATE_CHAR_STANDING_IN_AREA_3D" || cmd == "LOCATE_CHAR_STANDING_IN_AREA_2D") {
				float x = 0, y = 0, z = 0;
				float r = 5.0f;
				skipToWhitespace(lp);
				skipWhitespace(lp);
				if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
					tryParseFloat(lp, r);
					tryParseFloat(lp, r);
					tryParseFloat(lp, r);
					addEntity(gEntities, ENTITY_LOCATE, x, y, z, "", scriptName.c_str(), "", lineNum, 0.0f, r);
				}
				continue;
			}

			if (cmd == "PRINT_BIG" || cmd == "PRINT" || cmd == "PRINT_NOW" ||
				cmd == "PRINT_WITH_NUMBER_BIG" || cmd == "PRINT_WITH_NUMBER" ||
				cmd == "PRINT_SOON" || cmd == "PRINT_SOON_NOW") {
				float x = 0, y = 0, z = 0;
				if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
					addEntity(gEntities, ENTITY_CHECKPOINT, x, y, z, "", scriptName.c_str(), "", lineNum);
				}
				continue;
			}

			if (cmd == "SET_FIXED_CAMERA_POSITION" || cmd == "SET_CAMERA_POSITION") {
				float x = 0, y = 0, z = 0;
				if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
					addEntity(gEntities, ENTITY_CAMERA, x, y, z, "", scriptName.c_str(), "", lineNum);
				}
				continue;
			}

			if (cmd == "EXTEND_ROUTE" || cmd == "EXTEND_ROUTE") {
				float x = 0, y = 0, z = 0;
				if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
					addEntity(gEntities, ENTITY_ROUTE, x, y, z, "", scriptName.c_str(), "", lineNum);
				}
				continue;
			}

			if (cmd == "SET_CHAR_COORDINATES" || cmd == "SET_CHAR_COORDINATES_DONT_WARP_GANG" ||
				cmd == "SET_CAR_COORDINATES" || cmd == "TASK_GO_STRAIGHT_TO_COORD") {
				float x = 0, y = 0, z = 0;
				skipToWhitespace(lp);
				skipWhitespace(lp);
				if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
					addEntity(gEntities, ENTITY_TELEPORT, x, y, z, "", scriptName.c_str(), "", lineNum);
				}
				continue;
			}

			for (auto& kv : coordVars) {
				std::string var = kv.first;
				std::string assign = var + "=";
				if (cmd == assign || cmd == var) {
					skipWhitespace(lp);
					float val;
					if (tryParseFloat(lp, val)) {
						coordVars[var] = val;
					}
					break;
				}
			}
		}

		free(buffer);
	} while (FindNextFileA(hFind, &findData));

	FindClose(hFind);

	log("ScriptEntities: loaded %d entities\n", (int)gEntities.size());
}

void
ScriptEntities::Render(void)
{
	if (!gRenderScriptEntities || gEntities.empty())
		return;

	rw::V3d camPos = TheCamera.m_position;

	for (size_t i = 0; i < gEntities.size(); i++) {
		ScriptEntity& e = gEntities[i];

		float dx = e.x - camPos.x;
		float dy = e.y - camPos.y;
		float dz = e.z - camPos.z;
		float dist = sqrtf(dx*dx + dy*dy + dz*dz);
		if (dist > gScriptLabelDistance)
			continue;

		bool shouldRender = false;
		switch (e.type) {
			case ENTITY_CAR: shouldRender = gRenderScriptCars; break;
			case ENTITY_PED: shouldRender = gRenderScriptPeds; break;
			case ENTITY_OBJECT: shouldRender = gRenderScriptObjects; break;
			case ENTITY_PICKUP: shouldRender = gRenderScriptPickups; break;
			case ENTITY_BLIP: shouldRender = gRenderScriptBlips; break;
			case ENTITY_FX: shouldRender = gRenderScriptFx; break;
			case ENTITY_CHECKPOINT: shouldRender = gRenderScriptCheckpoints; break;
			case ENTITY_GENERATOR: shouldRender = gRenderScriptGenerators; break;
			case ENTITY_LOCATE: shouldRender = gRenderScriptLocates; break;
			case ENTITY_CAMERA: shouldRender = gRenderScriptCameras; break;
			case ENTITY_ROUTE: shouldRender = gRenderScriptRoutes; break;
			case ENTITY_TELEPORT: shouldRender = gRenderScriptTeleports; break;
		}

		if (!shouldRender) continue;

		float halfSize = gScriptCubeSize;
		float verticalOffset = halfSize * 3.0f;

		rw::RGBA col;
		switch (e.type) {
			case ENTITY_CAR: col = { 0, 100, 255, 200 }; break;
			case ENTITY_PED: col = { 255, 200, 0, 200 }; break;
			case ENTITY_OBJECT: col = { 200, 100, 0, 200 }; break;
			case ENTITY_PICKUP: col = { 255, 0, 255, 200 }; break;
			case ENTITY_BLIP: col = { 255, 255, 0, 200 }; break;
			case ENTITY_FX: col = { 255, 100, 50, 200 }; break;
			case ENTITY_CHECKPOINT: col = { 100, 255, 100, 200 }; break;
			case ENTITY_GENERATOR: col = { 100, 200, 255, 200 }; break;
			case ENTITY_LOCATE: col = { 200, 200, 100, 200 }; break;
			case ENTITY_CAMERA: col = { 255, 0, 200, 200 }; break;
			case ENTITY_ROUTE: col = { 0, 255, 200, 200 }; break;
			case ENTITY_TELEPORT: col = { 200, 0, 200, 200 }; break;
		}

		rw::V3d v[8] = {
			{ e.x - halfSize, e.y - halfSize, e.z },
			{ e.x + halfSize, e.y - halfSize, e.z },
			{ e.x - halfSize, e.y + halfSize, e.z },
			{ e.x + halfSize, e.y + halfSize, e.z },
			{ e.x - halfSize, e.y - halfSize, e.z + verticalOffset * 2.0f },
			{ e.x + halfSize, e.y - halfSize, e.z + verticalOffset * 2.0f },
			{ e.x - halfSize, e.y + halfSize, e.z + verticalOffset * 2.0f },
			{ e.x + halfSize, e.y + halfSize, e.z + verticalOffset * 2.0f }
		};

		RenderLine(v[0], v[1], col, col);
		RenderLine(v[2], v[3], col, col);
		RenderLine(v[4], v[5], col, col);
		RenderLine(v[6], v[7], col, col);
		RenderLine(v[0], v[2], col, col);
		RenderLine(v[1], v[3], col, col);
		RenderLine(v[4], v[6], col, col);
		RenderLine(v[5], v[7], col, col);
		RenderLine(v[0], v[4], col, col);
		RenderLine(v[1], v[5], col, col);
		RenderLine(v[2], v[6], col, col);
		RenderLine(v[3], v[7], col, col);

		rw::V3d worldPos = { e.x, e.y, e.z + verticalOffset * 1.5f };
		rw::V3d screenPos;
		float w, h;
		if (Sprite::CalcScreenCoors(worldPos, &screenPos, &w, &h, false)) {
			if (screenPos.z > 0.0f && screenPos.z < gTextFarClip) {
				char label[256];
				label[0] = '\0';

				switch (e.type) {
					case ENTITY_CAR: strncat(label, "[CAR] ", sizeof(label) - 1); break;
					case ENTITY_PED: strncat(label, "[PED] ", sizeof(label) - 1); break;
					case ENTITY_OBJECT: strncat(label, "[OBJ] ", sizeof(label) - 1); break;
					case ENTITY_PICKUP: strncat(label, "[PICKUP] ", sizeof(label) - 1); break;
					case ENTITY_BLIP: strncat(label, "[BLIP] ", sizeof(label) - 1); break;
					case ENTITY_FX: strncat(label, "[FX] ", sizeof(label) - 1); break;
					case ENTITY_CHECKPOINT: strncat(label, "[CHECK] ", sizeof(label) - 1); break;
					case ENTITY_GENERATOR: strncat(label, "[GEN] ", sizeof(label) - 1); break;
					case ENTITY_LOCATE: strncat(label, "[LOC] ", sizeof(label) - 1); break;
					case ENTITY_CAMERA: strncat(label, "[CAM] ", sizeof(label) - 1); break;
					case ENTITY_ROUTE: strncat(label, "[ROUTE] ", sizeof(label) - 1); break;
					case ENTITY_TELEPORT: strncat(label, "[TELE] ", sizeof(label) - 1); break;
				}

				if (gRenderScriptModelName && e.modelName[0]) {
					strncat(label, e.modelName, sizeof(label) - 1);
					strncat(label, " ", sizeof(label) - 1);
				}

				if (gRenderScriptFileName && e.scriptName[0]) {
					strncat(label, e.scriptName, sizeof(label) - 1);
					strncat(label, " ", sizeof(label) - 1);
				}

				if (gRenderScriptLineNumber) {
					char tmp[32];
					snprintf(tmp, sizeof(tmp), "L%d ", e.lineNum);
					strncat(label, tmp, sizeof(label) - 1);
				}

				char tmp[64];
				snprintf(tmp, sizeof(tmp), "%.0f,%.0f,%.0f", e.x, e.y, e.z);
				strncat(label, tmp, sizeof(label) - 1);

				ImU32 labelCol = IM_COL32(255, 255, 0, 255);
				ImFont* font = ImGui::GetFont();
				float fontSize = ImGui::GetFontSize();
				ImVec2 textSize = ImGui::CalcTextSize(label);
				float x = screenPos.x - textSize.x * 0.5f;
				float y = screenPos.y - textSize.y;
				ImDrawList* drawList = ImGui::GetBackgroundDrawList();
				drawList->AddText(font, fontSize, ImVec2(x + 1.0f, y + 1.0f), IM_COL32_BLACK, label);
				drawList->AddText(font, fontSize, ImVec2(x, y), labelCol, label);
			}
		}
	}
}

void
ScriptEntities::Shutdown(void)
{
	gEntities.clear();
}

int
ScriptEntities::GetNumEntities(void)
{
	return (int)gEntities.size();
}

ScriptEntity*
ScriptEntities::GetEntity(int index)
{
	if (index >= 0 && index < (int)gEntities.size())
		return &gEntities[index];
	return nil;
}
