#include "euryopa.h"
#include "scriptentities.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

std::vector<ScriptEntity> gEntities;
std::vector<ScriptFile> gScriptFiles;

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
bool ScriptEntities::gRenderScriptPlayers = true;
bool ScriptEntities::gRenderScriptGangCars = true;
bool ScriptEntities::gRenderScriptGarages = true;
bool ScriptEntities::gRenderScriptPathPoints = true;
bool ScriptEntities::gRenderScriptFires = true;
bool ScriptEntities::gRenderScriptExplosions = true;
bool ScriptEntities::gRenderScriptSearchlights = true;
bool ScriptEntities::gRenderScriptCoronas = true;
bool ScriptEntities::gRenderScriptMarkers = true;
bool ScriptEntities::gRenderScriptDoors = true;
bool ScriptEntities::gRenderScriptRemoteCars = true;
bool ScriptEntities::gRenderScriptTrains = true;
bool ScriptEntities::gRenderScriptBoats = true;
bool ScriptEntities::gRenderScriptHelis = true;
bool ScriptEntities::gRenderScriptWeathers = true;
bool ScriptEntities::gRenderScriptZones = true;
bool ScriptEntities::gRenderScriptSpawns = true;
bool ScriptEntities::gRenderScriptProjectiles = true;
bool ScriptEntities::gRenderScriptAudio = true;
bool ScriptEntities::gRenderScriptDraw = true;
bool ScriptEntities::gRenderScriptTasks = true;
bool ScriptEntities::gRenderScriptDamage = true;
bool ScriptEntities::gRenderScriptMission = true;

float ScriptEntities::gScriptLabelDistance = 200.0f;
float ScriptEntities::gScriptCubeDistance = 400.0f;
float ScriptEntities::gScriptCubeSize = 0.5f;
bool ScriptEntities::gRenderScriptText = true;
bool ScriptEntities::gRenderScriptFileName = true;
bool ScriptEntities::gRenderScriptModelName = true;
bool ScriptEntities::gRenderScriptLineNumber = false;
bool ScriptEntities::gRenderScriptComment = true;
int ScriptEntities::gMaxScriptLabels = 100;

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
					 const char* modelName, const char* scriptName, const char* varName, const char* comment, int lineNum, float heading = 0.0f, float radius = 0.0f) {
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
	if (comment) {
		strncpy(e.comment, comment, sizeof(e.comment) - 1);
		e.comment[sizeof(e.comment) - 1] = '\0';
	} else {
		e.comment[0] = '\0';
	}
	ents.push_back(e);
}

static const char* extractComment(const char* line) {
	const char* comment = strstr(line, "//");
	if (comment) {
		while (*comment == ' ' || *comment == '\t') comment++;
		return comment;
	}
	return "";
}

static void parseScFile(const char* filepath, const char* baseDir, const char* filename, std::map<std::string, float>& coordVars, int fileIndex);

void
ScriptEntities::Init(void)
{
	gEntities.clear();
	gScriptFiles.clear();
	log("ScriptEntities: Init - parsing scripts folder\n");

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
					ScriptFile sf;
					strncpy(sf.fullPath, fullPath, sizeof(sf.fullPath) - 1);
					sf.fullPath[sizeof(sf.fullPath) - 1] = '\0';
					strncpy(sf.filename, name, sizeof(sf.filename) - 1);
					sf.filename[sizeof(sf.filename) - 1] = '\0';
					sf.enabled = true;
					sf.numEntities = 0;
					sf.entityIndices.clear();
					gScriptFiles.push_back(sf);
				}
			}
		} while (FindNextFileA(hFind, &findData));

		FindClose(hFind);
		processed++;
	}

	for (size_t i = 0; i < gScriptFiles.size(); i++) {
		parseScFile(gScriptFiles[i].fullPath, baseDir, gScriptFiles[i].filename, coordVars, (int)i);
	}

	log("ScriptEntities: loaded %d entities from %d files\n", (int)gEntities.size(), (int)gScriptFiles.size());
}

void
ScriptEntities::Reload(void)
{
	Init();
}

void
ScriptEntities::TeleportToEntity(int entityIndex)
{
	if (entityIndex < 0 || entityIndex >= (int)gEntities.size())
		return;
	ScriptEntity& e = gEntities[entityIndex];
	TeleportToCoords(e.x, e.y, e.z);
}

void
ScriptEntities::TeleportToCoords(float x, float y, float z)
{
	TheCamera.m_position = { x, y, z };
	TheCamera.m_target = { x, y, z + 10.0f };
}

static void parseScFile(const char* filepath, const char* baseDir, const char* filename, std::map<std::string, float>& coordVars, int fileIndex)
{
	FILE* f = fopen(filepath, "r");
	if (!f) return;

	int startEntityIndex = (int)gEntities.size();

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
				addEntity(gEntities, ENTITY_CAR, x, y, z, model.c_str(), scriptName.c_str(), "", currentLine.c_str(), lineNum, h);
			}
			continue;
		}

		if (cmd == "CREATE_PLAYER" || cmd == "CREATE_PLAYER_FOR_CUTSCENE") {
			skipToWhitespace(lp);
			skipWhitespace(lp);
			
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_PLAYER, x, y, z, "PLAYER", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "CREATE_CHAR" || cmd == "CREATE_CHAR_PED" || cmd == "CREATE_CHAR_INSIDE_CAR" ||
			cmd == "CREATE_CHAR_AS_PASSENGER" || cmd == "CREATE_CHAR_AS_DRIVER" ||
			cmd == "CREATE_CHAR_IN_CAR") {
			skipWhitespace(lp);
			skipToWhitespace(lp);
			skipWhitespace(lp);
			std::string model = getNextWord(lp);
			
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_PED, x, y, z, model.c_str(), scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "CREATE_GANG_CAR" || cmd == "CREATE_CAR_GENERATOR_WITH_MODEL") {
			skipWhitespace(lp);
			std::string model = getNextWord(lp);
			
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_GANG_CAR, x, y, z, model.c_str(), scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "CREATE_OBJECT" || cmd == "CREATE_OBJECT_NO_ROTATE" ||
			cmd == "CREATE_OBJECT_INTSA" || cmd == "CREATE_RADAR_MARKER" ||
			cmd == "CREATE_OBJECT_NO_OFFSET" || cmd == "CREATE_OBJECT_IN_CAR") {
			skipWhitespace(lp);
			std::string model = getNextWord(lp);
			
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_OBJECT, x, y, z, model.c_str(), scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "CREATE_PICKUP" || cmd == "CREATE_PICKUP_MONEY" ||
			cmd == "CREATE_AMMO_PICKUP" || cmd == "CREATE_WEAPON_PICKUP" ||
			cmd == "CREATE_FLOATING_BLIP_2D" || cmd == "CREATE_PICKUP_WITH_ANGLE" ||
			cmd == "CREATE_MONEY_PICKUP" || cmd == "CREATE_PICKUP_SPECIAL" ||
			cmd == "CREATE_ASSET_PICKUP" || cmd == "CREATE_PICKUP_MONEY_WITH_TIME" ||
			cmd == "CREATE_HIDDEN_PACKAGE" || cmd == "CREATE_SPECIAL" ||
			cmd == "CREATE_PICKUP_IN_SHOP" || cmd == "CREATE_ITEM_PICKUP" ||
			cmd == "CREATE_PICKUP_MONEY_WITH_HEADSHOT" || cmd == "CREATE_SCRIPT_PICKUP" ||
			cmd == "CREATE_PICKUP_MONEY_NO_PROMPT") {
			skipWhitespace(lp);
			std::string model = getNextWord(lp);
			skipWhitespace(lp);
			std::string pickupType = getNextWord(lp);
			
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_PICKUP, x, y, z, model.c_str(), scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "ADD_BLIP_FOR_COORD" || cmd == "ADD_BLIP_FOR_COORD_2" ||
			cmd == "ADD_SHORT_RANGE_BLIP_FOR_COORD" || cmd == "ADD_MISSION_BLIP_FOR_COORD" ||
			cmd == "ADD_SCRIPTED_SPRITE" || cmd == "SET_COORD_BLIP_TEMP" ||
			cmd == "ADD_BLIP_FOR_CONTACT_POINT" || cmd == "ADD_BLIP_FOR_OBJECT") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_BLIP, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "CREATE_FX_SYSTEM" || cmd == "CREATE_FX_SYSTEM_PP" || 
			cmd == "CREATE_FX_SYSTEM_W_ember" || cmd == "CREATE_FX") {
			std::string fxName = getNextWord(lp);
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_FX, x, y, z, fxName.c_str(), scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "CREATE_EXPLOSION" || cmd == "CREATE_MOLOTOV") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_EXPLOSION, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "START_SCRIPT_FIRE" || cmd == "CREATE_SCRIPT_FIRE") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_FIRE, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "SWITCH_CAR_GENERATOR" || cmd == "CREATE_CAR_GENERATOR") {
			float x = 0, y = 0, z = 0;
			std::string dummy;
			if (tryParseFloat(lp, x)) {
				y = x;
				z = x;
				addEntity(gEntities, ENTITY_GENERATOR, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
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
			cmd == "LOCATE_CHAR_STANDING_IN_AREA_3D" || cmd == "LOCATE_CHAR_STANDING_IN_AREA_2D" ||
			cmd == "LOCATE_PLAYER_ANY_MEANS_3D" || cmd == "LOCATE_PLAYER_ANY_MEANS_2D" ||
			cmd == "LOCATE_PLAYER_ON_FOOT_3D" || cmd == "LOCATE_PLAYER_ON_FOOT_2D" ||
			cmd == "LOCATE_PLAYER_IN_CAR_3D" || cmd == "LOCATE_PLAYER_IN_CAR_2D" ||
			cmd == "LOCATE_OBJECT_3D" || cmd == "LOCATE_OBJECT_2D" ||
			cmd == "LOCATE_OBJECT_IN_CAR_3D" || cmd == "LOCATE_STOPPED_CAR_3D" ||
			cmd == "LOCATE_STOPPED_CHAR_ON_FOOT_3D" || cmd == "LOCATE_STOPPED_CHAR_ON_FOOT_2D") {
			float x = 0, y = 0, z = 0;
			float r = 5.0f;
			skipToWhitespace(lp);
			skipWhitespace(lp);
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				tryParseFloat(lp, r);
				tryParseFloat(lp, r);
				tryParseFloat(lp, r);
				addEntity(gEntities, ENTITY_LOCATE, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum, 0.0f, r);
			}
			continue;
		}

		if (cmd == "PRINT_BIG" || cmd == "PRINT" || cmd == "PRINT_NOW" ||
			cmd == "PRINT_WITH_NUMBER_BIG" || cmd == "PRINT_WITH_NUMBER" ||
			cmd == "PRINT_SOON" || cmd == "PRINT_SOON_NOW" ||
			cmd == "PRINT_WITH_MESSAGE_BIG" || cmd == "PRINT_WITH_2_NUMBERS" ||
			cmd == "PRINT_STRING" || cmd == "PRINT_FORMATTED_HELP" ||
			cmd == "PRINT_HELP" || cmd == "FLASH_HUD" ||
			cmd == "PRINT_TEXT_TWO_STRINGS" || cmd == "PRINT_BRIEF" ||
			cmd == "PRINT_MISSION_TEXT" || cmd == "PRINT_END_JUST_FORE" ||
			cmd == "CLEAR_THIS_PRINT" || cmd == "CLEAR_SMALL_PRINTS" ||
			cmd == "CLEAR_ALL_HELP_PRINTS" || cmd == "IS_MESSAGE_DISPLAYED" ||
			cmd == "ADD_BLIP_FOR_COORD_2" || cmd == "ADD_UPSIDEDOWN_BLIP") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_CHECKPOINT, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "SET_FIXED_CAMERA_POSITION" || cmd == "SET_CAMERA_POSITION" ||
			cmd == "POINT_CAMERA_AT_POINT" || cmd == "RESTORE_CAMERA" ||
			cmd == "SET_CAMERA_BEHIND_PLAYER" || cmd == "SHAKE_CAMERA" ||
			cmd == "FADE_CAMERA" || cmd == "FORCE_CAMERA" ||
			cmd == "ATTACH_CAMERA_TO_VEHICLE_LOOK_AT_COORD") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_CAMERA, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "EXTEND_ROUTE" || cmd == "ADD_PATH_POINT" ||
			cmd == "ADD_ROUTE_POINT" || cmd == "CREATE_PATH_IN_STATION" ||
			cmd == "ADD_PATH_POINT_NODES") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_ROUTE, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "SET_CHAR_COORDINATES" || cmd == "SET_CHAR_COORDINATES_DONT_WARP_GANG" ||
			cmd == "SET_CAR_COORDINATES" || cmd == "TASK_GO_STRAIGHT_TO_COORD" ||
			cmd == "SET_PLAYER_COORDINATES" || cmd == "SET_PLAYER_COORDINATES_DONT_WARP_GANG" ||
			cmd == "WARP_CHAR_FROM_CAR_TO_COORDS" || cmd == "WARP_PLAYER_FROM_CAR_TO_COORD" ||
			cmd == "SET_OBJECT_COORDINATES" || cmd == "TELEPORT_OBJECT") {
			float x = 0, y = 0, z = 0;
			skipToWhitespace(lp);
			skipWhitespace(lp);
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_TELEPORT, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "CREATE_GARAGE" || cmd == "CLOSE_GARAGE" || cmd == "OPEN_GARAGE" ||
			cmd == "CHANGE_GARAGE_TYPE" || cmd == "SET_GARAGE_RESpawn_Time" ||
			cmd == "SET_GARAGE_CAR_GENERATOR" || cmd == "ACTIVATE_GARAGE" ||
			cmd == "DEACTIVATE_GARAGE" || cmd == "DRAW_SHADOW" ||
			cmd == "CREATE_SCRIPT_SPIDER" || cmd == "ACTIVATE_SCRIPT_SPIDER") {
			float x1 = 0, y1 = 0, z1 = 0, x2 = 0, y2 = 0, z2 = 0;
			if (tryParseFloat(lp, x1) && tryParseFloat(lp, y1) && tryParseFloat(lp, z1) &&
				tryParseFloat(lp, x2) && tryParseFloat(lp, y2) && tryParseFloat(lp, z2)) {
				float cx = (x1 + x2) / 2.0f;
				float cy = (y1 + y2) / 2.0f;
				float cz = (z1 + z2) / 2.0f;
				addEntity(gEntities, ENTITY_GARAGE, cx, cy, cz, "", scriptName.c_str(), "", currentLine.c_str(), lineNum, 0.0f, 10.0f);
			}
			continue;
		}

		if (cmd == "CREATE_SEARCHLIGHT" || cmd == "CREATE_SEARCHLIGHT_NO_ANGLE") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_SEARCHLIGHT, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "DRAW_CORONA" || cmd == "DRAW_LIGHT" || cmd == "CREATE_LIGHT" ||
			cmd == "CREATE_LIGHT_WITH_ANGLE" || cmd == "DRAW_SPRING") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_CORONA, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "CREATE_MARKER" || cmd == "CREATE_MARKER_SHORT" ||
			cmd == "CREATE_MARKER_AT_3D_COORD" || cmd == "CREATE_RADAR_MARKER" ||
			cmd == "CREATE_ICON_MARKER" || cmd == "CREATE_COLOUR_MARKER" ||
			cmd == "CREATE_RADAR_MARKER_COLORED" || cmd == "CREATE_MARKER_BIG" ||
			cmd == "CREATE_PICKUP_WITH_ANGLE") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_MARKER, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "OPEN_DOOR" || cmd == "CLOSE_DOOR" || cmd == "LOCK_DOOR" ||
			cmd == "UNLOCK_DOOR" || cmd == "SET_DOOR_OPEN" || cmd == "GET_DOOR_OPEN" ||
			cmd == "DOOR_AJAR_ANGLE" || cmd == "ROTATE_DOOR") {
			float x = 0, y = 0, z = 0;
			skipToWhitespace(lp);
			skipWhitespace(lp);
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_DOOR, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "GIVE_REMOTE_CONTROLLED_CAR_TO_PLAYER" || cmd == "CREATE_REMOTE_CONTROLLED_CAR" ||
			cmd == "START_KILL_FRENZY" || cmd == "CREATE_KILL_FRENZY") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_REMOTE_CAR, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "TRAIN_CREATED_BY_SCRIPT" || cmd == "CREATE_TRAIN" ||
			cmd == "ATTACH_TRAIN_TO_CABRIO" || cmd == "DETACH_TRAIN" ||
			cmd == "SET_TRAIN_CABRIO" || cmd == "SET_TRAIN_SPEED" ||
			cmd == "SET_TRAIN_CRUISE_SPEED" || cmd == "DELETE_MISSION_TRAINS" ||
			cmd == "DELETE_ALL_TRAINS" || cmd == "MARK_MISSION_TRAINS_AS_NO_LONGER_NEEDED") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_TRAIN, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "SET_BOAT_CRUISE_SPEED" || cmd == "CREATE_BOAT" ||
			cmd == "DELETE_MISSION_BOATS" || cmd == "DELETE_ALL_BOATS" ||
			cmd == "MARK_BOAT_AS_MISSION_BOAT" || cmd == "SET_BOAT_SPECIAL" ||
			cmd == "BOAT_GROUND_BEHIND_BOAT" || cmd == "GET_BOAT_CRUISE_SPEED") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_BOAT, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "CREATE_HELI" || cmd == "CREATE_PLANE" || cmd == "CREATE_JETPACK" ||
			cmd == "CONTROL_HELI_BLADES" || cmd == "SET_HELI_TURRET_STRENGTH" ||
			cmd == "SET_HELI_STABILiser" || cmd == "DETACH_HELI" ||
			cmd == "HELI_LANDING" || cmd == "SET_FLYING_THROUGH_WINDSCREEN") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_HELI, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "SET_WEATHER" || cmd == "FORCE_WEATHER" || cmd == "RELEASE_WEATHER" ||
			cmd == "SET_WEATHER_NOW" || cmd == "SET_PERSISTENT_WEATHER") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_WEATHER, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "SET_ZONE_CIVILIAN_CAR_DENSITY" || cmd == "SET_ZONE_NO_Cops" ||
			cmd == "GET_ZONE_COP_PEDMODEL" || cmd == "SET_ZONE_GANG_DENSITY" ||
			cmd == "SET_ZONE_FAVOURITE_3" || cmd == "SET_ZONE_NO_GANGZ" ||
			cmd == "SET_AREA_FLAG" || cmd == "GET_AREA_FLAG") {
			float x = 0, y = 0, z = 0;
			skipToWhitespace(lp);
			skipWhitespace(lp);
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_ZONE, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "GIVE_WEAPON" || cmd == "ADD_AMMO" || cmd == "SET_CURRENT_WANTED_LEVEL" ||
			cmd == "SET_CHAR_INVINCIBLE" || cmd == "GIVE_AMMO" || cmd == "SET_ARMOUR" ||
			cmd == "SET_HEALTH" || cmd == "ADD_HEALTH_MONEY" || cmd == "ADD_SCORE") {
			float x = 0, y = 0, z = 0;
			skipToWhitespace(lp);
			skipWhitespace(lp);
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_SPAWN, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "THROW_PROJECTILE" || cmd == "FIRE_PROJECTILE" ||
			cmd == "FIRE_PED_WEAPON" || cmd == "SHOOT_FIXED_CAR_GUN" ||
			cmd == "SHOOT_AVOID_PED" || cmd == "SHOOT_AT_COORD" ||
			cmd == "FIRE_PED_WEAPON_AT_CHAR" || cmd == "FIRE_PED_WEAPON_AT_CAR" ||
			cmd == "CREATE_PROJECTILE" || cmd == "CREATE_PROJECTILE_FROM_CAR") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_PROJECTILE, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "CREATE_RANDOM_PED" || cmd == "CREATE_RANDOM_CHAR" ||
			cmd == "CREATE_RANDOM_CAR" || cmd == "CREATE_RANDOM_VEHICLE" ||
			cmd == "CREATE_RANDOM_BOAT" || cmd == "CREATE_RANDOM_TRAIN") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_PED, x, y, z, "RANDOM", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "STORE_COORDINATES" || cmd == "STORE_CHAR_COORDINATES" ||
			cmd == "STORE_PLAYER_COORDINATES" || cmd == "STORE_CAR_COORDINATES" ||
			cmd == "STORE_OBJECT_COORDINATES" || cmd == "GET_CHAR_COORDINATES" ||
			cmd == "GET_CAR_COORDINATES" || cmd == "GET_PLAYER_COORDINATES" ||
			cmd == "GET_OBJECT_COORDINATES" || cmd == "STORE_POS_XYZ" ||
			cmd == "GET_OFFSET_COORDINATES" || cmd == "GET_CHAR_DISTANCE_FROM_CHAR" ||
			cmd == "GET_DISTANCE_FROM_CAR" || cmd == "GET_DISTANCE_FROM_POINT" ||
			cmd == "LOCATE_CHAR_ANY_MEANS_CHAR_2D" || cmd == "LOCATE_CHAR_ANY_MEANS_CHAR_3D" ||
			cmd == "LOCATE_CHAR_ON_FOOT_CHAR_2D" || cmd == "LOCATE_CHAR_ON_FOOT_CHAR_3D" ||
			cmd == "LOCATE_CHAR_IN_CAR_CHAR_2D" || cmd == "LOCATE_CHAR_IN_CAR_CHAR_3D" ||
			cmd == "LOCATE_CHAR_STANDING_CHAR_2D" || cmd == "LOCATE_CHAR_STANDING_CHAR_3D" ||
			cmd == "LOCATE_CHAR_IN_AREA_2D" || cmd == "LOCATE_CHAR_IN_AREA_3D" ||
			cmd == "LOCATE_CHAR_ANY_MEANS_2D" || cmd == "LOCATE_CHAR_ANY_MEANS_3D" ||
			cmd == "LOCATE_CHAR_ON_FOOT_2D" || cmd == "LOCATE_CHAR_ON_FOOT_3D" ||
			cmd == "LOCATE_CHAR_IN_CAR_2D" || cmd == "LOCATE_CHAR_IN_CAR_3D" ||
			cmd == "LOCATE_CHAR_STANDING" || cmd == "LOCATE_CHAR_STANDING_IN_AREA_2D" ||
			cmd == "LOCATE_CHAR_STANDING_IN_AREA_3D" || cmd == "LOCATE_CAR_2D" ||
			cmd == "LOCATE_CAR_3D" || cmd == "LOCATE_STOPPED_CHAR_ANY_MEANS_2D" ||
			cmd == "LOCATE_STOPPED_CHAR_ANY_MEANS_3D" || cmd == "LOCATE_STOPPED_CHAR_ON_FOOT_2D" ||
			cmd == "LOCATE_STOPPED_CHAR_ON_FOOT_3D" || cmd == "LOCATE_STOPPED_CAR_2D" ||
			cmd == "LOCATE_STOPPED_CAR_3D" || cmd == "LOCATE_STOPPED_CHAR_IN_CAR_2D" ||
			cmd == "LOCATE_STOPPED_CHAR_IN_CAR_3D") {
			float x = 0, y = 0, z = 0;
			skipToWhitespace(lp);
			skipWhitespace(lp);
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_LOCATE, x, y, z, "", scriptName.c_str(), "", lineNum);
			}
			continue;
		}

		if (cmd == "START_KILL_FRENZY" || cmd == "CREATE_KILL_FRENZY" ||
			cmd == "START_KILL_FRENZY_2" || cmd == "START_KILL_FRENZY_3" ||
			cmd == "START_KILL_FRENZY_4" || cmd == "START_KILL_FRENZY_5") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_MARKER, x, y, z, "FRENZY", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "PLAY_MISSION_AUDIO" || cmd == "LOAD_MISSION_AUDIO" ||
			cmd == "SET_MISSION_AUDIO_POSITION" || cmd == "PLAY_LOADED_MISSION_AUDIO" ||
			cmd == "PLAY_MISSION_AUDIO_THEN_PAUSES") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_AUDIO, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "DRAW_SPHERE" || cmd == "DRAW_SPHERE_AT" ||
			cmd == "DRAW_CORONA_AT" || cmd == "DRAW_SPRITE_AT" ||
			cmd == "DRAW_SPRITE_WITH_ANGLE" || cmd == "DRAW_SPRITE_NIT" ||
			cmd == "DRAW_SPRITE_2D" || cmd == "DRAW_TEXTURE" ||
			cmd == "DRAW_RECT" || cmd == "DRAW_DEBUG_CUBE") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_DRAW, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "TASK_GO_STRAIGHT_TO_COORD" || cmd == "TASK_GO_TO_COORD" ||
			cmd == "TASK_GO_TO_COORD_IN_CAR" || cmd == "TASK_LEAVE_CAR" ||
			cmd == "TASK_LEAVE_ANY_CAR" || cmd == "TASK_WANDER" ||
			cmd == "TASK_WANDER_FROM_COORD" || cmd == "TASK_FOLLOW_PATH" ||
			cmd == "TASK_SHOOT" || cmd == "TASK_KILL_CHAR_FROM_CAR" ||
			cmd == "TASK_KILL_CHAR_ON_FOOT" || cmd == "TASK_DIE" ||
			cmd == "TASK_DUCK" || cmd == "TASK_STAND_STILL" ||
			cmd == "TASK_LOOK_AT_CHAR" || cmd == "TASK_LOOK_AT_PLAYER" ||
			cmd == "TASK_LOOK_AT_CAR" || cmd == "TASK_AIM_GUN_AT_CHAR" ||
			cmd == "TASK_AIM_GUN_AT_PLAYER" || cmd == "TASK_AIM_GUN_AT_CAR" ||
			cmd == "TASK_TIRED" || cmd == "TASK_SIT_DOWN" ||
			cmd == "TASK_COWER" || cmd == "TASK_HANDS_UP" ||
			cmd == "TASK_FOLLOW_FORMATION") {
			float x = 0, y = 0, z = 0;
			skipToWhitespace(lp);
			skipWhitespace(lp);
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_TASK, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "SET_CHAR_HEADING" || cmd == "SET_CAR_HEADING" ||
			cmd == "SET_OBJECT_HEADING" || cmd == "SET_PLAYER_HEADING" ||
			cmd == "SET_CHAR_COORDINATES_NO_OFFSET" || cmd == "SET_CAR_COORDINATES_NO_OFFSET" ||
			cmd == "SET_CHAR_POSITION" || cmd == "SET_CAR_POSITION" ||
			cmd == "SET_CHAR_VELOCITY" || cmd == "SET_CAR_VELOCITY" ||
			cmd == "SET_CHAR_PROOFS" || cmd == "SET_CAR_PROOFS") {
			float x = 0, y = 0, z = 0;
			skipToWhitespace(lp);
			skipWhitespace(lp);
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_TELEPORT, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "SET_CHAR_HEALTH" || cmd == "SET_CAR_HEALTH" ||
			cmd == "ADD_HEALTH" || cmd == "ADD_ARMOUR" ||
			cmd == "SET_CHAR_ARMOUR" || cmd == "SET_CHAR_MAX_HEALTH" ||
			cmd == "SET_CAR_MAX_HEALTH" || cmd == "DAMAGE_CAR" ||
			cmd == "DAMAGE_CHAR" || cmd == "EXPLODE_CAR" ||
			cmd == "EXPLODE_CHAR") {
			float x = 0, y = 0, z = 0;
			skipToWhitespace(lp);
			skipWhitespace(lp);
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_DAMAGE, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "PLAY_SOUND" || cmd == "PLAY_SOUND_FROM_PLAYER" ||
			cmd == "PLAY_SOUND_FROM_CAR" || cmd == "PLAY_SOUND_FROM_CHAR" ||
			cmd == "PLAY_SOUND_FROM_OBJECT" || cmd == "LOAD_SOUND" ||
			cmd == "RELEASE_SOUND" || cmd == "STOP_SOUND" ||
			cmd == "PLAY_MUSIC" || cmd == "STOP_MUSIC" ||
			cmd == "PLAY_CONCERTED_SOUND" || cmd == "PLAY_FRONT_END_SOUND") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_AUDIO, x, y, z, "", scriptName.c_str(), "", lineNum);
			}
			continue;
		}

		if (cmd == "CREATE_PED_WITH_GUN" || cmd == "CREATE_MISSION_PED" ||
			cmd == "CREATE_ESCAPE_PARAM_PED" || cmd == "CREATE_SWAT_MEMBER" ||
			cmd == "CREATE_FBI_MEMBER" || cmd == "CREATE_ARMY_MEMBER" ||
			cmd == "CREATE_HVY_STEALTH_MOTO" || cmd == "CREATE_STEALTH_MOTO" ||
			cmd == "CREATE_MISSION_CAR" || cmd == "CREATE_MISSION_CAR_NO_SAVE" ||
			cmd == "CREATE_CAR_UNTIL_CAR_IS_DELETE" || cmd == "CREATE_CAR_UNTIL_NO_LONGER_NEEDED" ||
			cmd == "CREATE_OBJECT_UNTIL_CAR_IS_DELETE" || cmd == "CREATE_OBJECT_UNTIL_NO_LONGER_NEEDED" ||
			cmd == "CREATE_CHAR_UNTIL_NO_LONGER_NEEDED" || cmd == "CREATE_CHAR_UNTIL_CAR_IS_DELETE") {
			float x = 0, y = 0, z = 0;
			skipToWhitespace(lp);
			skipWhitespace(lp);
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_MISSION, x, y, z, "", scriptName.c_str(), "", currentLine.c_str(), lineNum);
			}
			continue;
		}

		if (cmd == "TRIGGER_MISSION_COMPLETED_SOUND" || cmd == "TRIGGER_MISSION_FAILED_SOUND" ||
			cmd == "TRIGGERPolice_BAITS" || cmd == "TRIGGER_GARAGE_OPENED_SOUND" ||
			cmd == "TRIGGER_GARAGE_CLOSED_SOUND" || cmd == "TRIGGER_AUDIO_EVENT" ||
			cmd == "TRIGGER_BANK_WARNING_SOUND") {
			float x = 0, y = 0, z = 0;
			if (tryParseFloat(lp, x) && tryParseFloat(lp, y) && tryParseFloat(lp, z)) {
				addEntity(gEntities, ENTITY_AUDIO, x, y, z, "TRIGGER", scriptName.c_str(), "", currentLine.c_str(), lineNum);
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

	if (fileIndex >= 0 && fileIndex < (int)gScriptFiles.size()) {
		gScriptFiles[fileIndex].numEntities = (int)gEntities.size() - startEntityIndex;
		for (int i = startEntityIndex; i < (int)gEntities.size(); i++) {
			gScriptFiles[fileIndex].entityIndices.push_back(i);
		}
	}
}

void
ScriptEntities::Render(void)
{
	if (!gRenderScriptEntities || gEntities.empty())
		return;

	rw::V3d camPos = TheCamera.m_position;
	int labelsRendered = 0;

	for (size_t i = 0; i < gEntities.size(); i++) {
		ScriptEntity& e = gEntities[i];

		if (!gScriptFiles.empty()) {
			bool fileEnabled = false;
			for (auto& sf : gScriptFiles) {
				if (sf.enabled) {
					for (int idx : sf.entityIndices) {
						if (idx == (int)i) {
							fileEnabled = true;
							break;
						}
					}
				}
				if (fileEnabled) break;
			}
			if (!fileEnabled) continue;
		}

		float dx = e.x - camPos.x;
		float dy = e.y - camPos.y;
		float dz = e.z - camPos.z;
		float dist = sqrtf(dx*dx + dy*dy + dz*dz);
		if (dist > gScriptCubeDistance)
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
			case ENTITY_PLAYER: shouldRender = gRenderScriptPlayers; break;
			case ENTITY_GANG_CAR: shouldRender = gRenderScriptGangCars; break;
			case ENTITY_GARAGE: shouldRender = gRenderScriptGarages; break;
			case ENTITY_PATH_POINT: shouldRender = gRenderScriptPathPoints; break;
			case ENTITY_FIRE: shouldRender = gRenderScriptFires; break;
			case ENTITY_EXPLOSION: shouldRender = gRenderScriptExplosions; break;
			case ENTITY_SEARCHLIGHT: shouldRender = gRenderScriptSearchlights; break;
			case ENTITY_CORONA: shouldRender = gRenderScriptCoronas; break;
			case ENTITY_MARKER: shouldRender = gRenderScriptMarkers; break;
			case ENTITY_DOOR: shouldRender = gRenderScriptDoors; break;
			case ENTITY_REMOTE_CAR: shouldRender = gRenderScriptRemoteCars; break;
			case ENTITY_TRAIN: shouldRender = gRenderScriptTrains; break;
			case ENTITY_BOAT: shouldRender = gRenderScriptBoats; break;
			case ENTITY_HELI: shouldRender = gRenderScriptHelis; break;
			case ENTITY_WEATHER: shouldRender = gRenderScriptWeathers; break;
			case ENTITY_ZONE: shouldRender = gRenderScriptZones; break;
			case ENTITY_SPAWN: shouldRender = gRenderScriptSpawns; break;
			case ENTITY_PROJECTILE: shouldRender = gRenderScriptProjectiles; break;
			case ENTITY_AUDIO: shouldRender = gRenderScriptAudio; break;
			case ENTITY_DRAW: shouldRender = gRenderScriptDraw; break;
			case ENTITY_TASK: shouldRender = gRenderScriptTasks; break;
			case ENTITY_DAMAGE: shouldRender = gRenderScriptDamage; break;
			case ENTITY_MISSION: shouldRender = gRenderScriptMission; break;
		}

		if (!shouldRender) continue;

		float halfSize = gScriptCubeSize;
		float verticalOffset = halfSize * 3.0f;

		if (e.type == ENTITY_GARAGE)
			verticalOffset = halfSize * 8.0f;
		if (e.type == ENTITY_ZONE)
			verticalOffset = halfSize * 15.0f;

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
			case ENTITY_PLAYER: col = { 0, 255, 0, 200 }; break;
			case ENTITY_GANG_CAR: col = { 255, 150, 0, 200 }; break;
			case ENTITY_GARAGE: col = { 150, 100, 50, 200 }; break;
			case ENTITY_PATH_POINT: col = { 100, 150, 100, 200 }; break;
			case ENTITY_FIRE: col = { 255, 100, 0, 200 }; break;
			case ENTITY_EXPLOSION: col = { 255, 50, 0, 200 }; break;
			case ENTITY_SEARCHLIGHT: col = { 255, 255, 200, 200 }; break;
			case ENTITY_CORONA: col = { 255, 255, 150, 200 }; break;
			case ENTITY_MARKER: col = { 255, 200, 100, 200 }; break;
			case ENTITY_DOOR: col = { 150, 100, 150, 200 }; break;
			case ENTITY_REMOTE_CAR: col = { 0, 200, 200, 200 }; break;
			case ENTITY_TRAIN: col = { 150, 150, 150, 200 }; break;
			case ENTITY_BOAT: col = { 0, 150, 255, 200 }; break;
			case ENTITY_HELI: col = { 200, 200, 100, 200 }; break;
			case ENTITY_WEATHER: col = { 150, 200, 255, 200 }; break;
			case ENTITY_ZONE: col = { 200, 100, 150, 200 }; break;
			case ENTITY_SPAWN: col = { 100, 255, 150, 200 }; break;
			case ENTITY_PROJECTILE: col = { 255, 100, 100, 200 }; break;
			case ENTITY_AUDIO: col = { 200, 100, 200, 200 }; break;
			case ENTITY_DRAW: col = { 255, 200, 50, 200 }; break;
			case ENTITY_TASK: col = { 150, 150, 200, 200 }; break;
			case ENTITY_DAMAGE: col = { 255, 50, 50, 200 }; break;
			case ENTITY_MISSION: col = { 255, 215, 0, 200 }; break;
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

		if (!gRenderScriptText || labelsRendered >= gMaxScriptLabels)
			continue;

		if (dist > gScriptLabelDistance)
			continue;

		labelsRendered++;

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
					case ENTITY_PLAYER: strncat(label, "[PLAYER] ", sizeof(label) - 1); break;
					case ENTITY_GANG_CAR: strncat(label, "[GANG] ", sizeof(label) - 1); break;
					case ENTITY_GARAGE: strncat(label, "[GARAGE] ", sizeof(label) - 1); break;
					case ENTITY_PATH_POINT: strncat(label, "[PATH] ", sizeof(label) - 1); break;
					case ENTITY_FIRE: strncat(label, "[FIRE] ", sizeof(label) - 1); break;
					case ENTITY_EXPLOSION: strncat(label, "[BOOM] ", sizeof(label) - 1); break;
					case ENTITY_SEARCHLIGHT: strncat(label, "[LIGHT] ", sizeof(label) - 1); break;
					case ENTITY_CORONA: strncat(label, "[CORONA] ", sizeof(label) - 1); break;
					case ENTITY_MARKER: strncat(label, "[MARKER] ", sizeof(label) - 1); break;
					case ENTITY_DOOR: strncat(label, "[DOOR] ", sizeof(label) - 1); break;
					case ENTITY_REMOTE_CAR: strncat(label, "[RC_CAR] ", sizeof(label) - 1); break;
					case ENTITY_TRAIN: strncat(label, "[TRAIN] ", sizeof(label) - 1); break;
					case ENTITY_BOAT: strncat(label, "[BOAT] ", sizeof(label) - 1); break;
					case ENTITY_HELI: strncat(label, "[HELI] ", sizeof(label) - 1); break;
					case ENTITY_WEATHER: strncat(label, "[WEATHER] ", sizeof(label) - 1); break;
					case ENTITY_ZONE: strncat(label, "[ZONE] ", sizeof(label) - 1); break;
					case ENTITY_SPAWN: strncat(label, "[SPAWN] ", sizeof(label) - 1); break;
					case ENTITY_PROJECTILE: strncat(label, "[PROJ] ", sizeof(label) - 1); break;
					case ENTITY_AUDIO: strncat(label, "[AUDIO] ", sizeof(label) - 1); break;
					case ENTITY_DRAW: strncat(label, "[DRAW] ", sizeof(label) - 1); break;
					case ENTITY_TASK: strncat(label, "[TASK] ", sizeof(label) - 1); break;
					case ENTITY_DAMAGE: strncat(label, "[DMG] ", sizeof(label) - 1); break;
					case ENTITY_MISSION: strncat(label, "[MSN] ", sizeof(label) - 1); break;
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

				if (gRenderScriptComment && e.comment[0]) {
					strncat(label, " ", sizeof(label) - 1);
					strncat(label, e.comment, sizeof(label) - 1);
				}

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
	gScriptFiles.clear();
}

int
ScriptEntities::GetNumEntities(void)
{
	return (int)gEntities.size();
}

int
ScriptEntities::GetNumFiles(void)
{
	return (int)gScriptFiles.size();
}

ScriptEntity*
ScriptEntities::GetEntity(int index)
{
	if (index >= 0 && index < (int)gEntities.size())
		return &gEntities[index];
	return nil;
}

ScriptFile*
ScriptEntities::GetFile(int index)
{
	if (index >= 0 && index < (int)gScriptFiles.size())
		return &gScriptFiles[index];
	return nil;
}

ScriptEntity*
ScriptEntities::GetEntityByFileAndIndex(int fileIndex, int entityInFileIndex)
{
	if (fileIndex >= 0 && fileIndex < (int)gScriptFiles.size()) {
		ScriptFile& sf = gScriptFiles[fileIndex];
		if (entityInFileIndex >= 0 && entityInFileIndex < (int)sf.entityIndices.size()) {
			int globalIndex = sf.entityIndices[entityInFileIndex];
			if (globalIndex >= 0 && globalIndex < (int)gEntities.size()) {
				return &gEntities[globalIndex];
			}
		}
	}
	return nil;
}

void
ScriptEntities::SetFileEnabled(int fileIndex, bool enabled)
{
	if (fileIndex >= 0 && fileIndex < (int)gScriptFiles.size()) {
		gScriptFiles[fileIndex].enabled = enabled;
	}
}

bool
ScriptEntities::IsFileEnabled(int fileIndex)
{
	if (fileIndex >= 0 && fileIndex < (int)gScriptFiles.size()) {
		return gScriptFiles[fileIndex].enabled;
	}
	return false;
}

const char*
ScriptEntities::GetEntityTypeName(ScriptEntityType type)
{
	switch (type) {
		case ENTITY_CAR: return "CAR";
		case ENTITY_PED: return "PED";
		case ENTITY_OBJECT: return "OBJECT";
		case ENTITY_PICKUP: return "PICKUP";
		case ENTITY_BLIP: return "BLIP";
		case ENTITY_FX: return "FX";
		case ENTITY_CHECKPOINT: return "CHECKPOINT";
		case ENTITY_GENERATOR: return "GENERATOR";
		case ENTITY_LOCATE: return "LOCATE";
		case ENTITY_CAMERA: return "CAMERA";
		case ENTITY_ROUTE: return "ROUTE";
		case ENTITY_TELEPORT: return "TELEPORT";
		case ENTITY_PLAYER: return "PLAYER";
		case ENTITY_GANG_CAR: return "GANG_CAR";
		case ENTITY_GARAGE: return "GARAGE";
		case ENTITY_PATH_POINT: return "PATH_POINT";
		case ENTITY_FIRE: return "FIRE";
		case ENTITY_EXPLOSION: return "EXPLOSION";
		case ENTITY_SEARCHLIGHT: return "SEARCHLIGHT";
		case ENTITY_CORONA: return "CORONA";
		case ENTITY_MARKER: return "MARKER";
		case ENTITY_DOOR: return "DOOR";
		case ENTITY_REMOTE_CAR: return "REMOTE_CAR";
		case ENTITY_TRAIN: return "TRAIN";
		case ENTITY_BOAT: return "BOAT";
		case ENTITY_HELI: return "HELI";
		case ENTITY_WEATHER: return "WEATHER";
		case ENTITY_ZONE: return "ZONE";
		case ENTITY_SPAWN: return "SPAWN";
		case ENTITY_PROJECTILE: return "PROJECTILE";
		case ENTITY_AUDIO: return "AUDIO";
		case ENTITY_DRAW: return "DRAW";
		case ENTITY_TASK: return "TASK";
		case ENTITY_DAMAGE: return "DAMAGE";
		case ENTITY_MISSION: return "MISSION";
		default: return "UNKNOWN";
	}
}
