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
bool ScriptEntities::gRenderScriptCoords = true;

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

static std::string getScriptFilename(const char* filepath) {
	const char* lastSlash = filepath;
	for (const char* p = filepath; *p; p++) {
		if (*p == '/' || *p == '\\') lastSlash = p + 1;
	}
	return std::string(lastSlash);
}

static void addEntity(std::vector<ScriptEntity>& ents, ScriptEntityType type, float x, float y, float z, 
					 const char* modelName, const char* scriptName, const char* varName, int lineNum, float heading = 0.0f) {
	ScriptEntity e;
	e.type = type;
	e.x = x;
	e.y = y;
	e.z = z;
	e.heading = heading;
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

	char path[512];
	snprintf(path, sizeof(path), "%s/scripts/*", exeDir);
	
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(path, &findData);
	if (hFind == INVALID_HANDLE_VALUE) {
		log("ScriptEntities: scripts folder not found at %s/scripts\n", exeDir);
		return;
	}

	char scriptsBaseDir[512];
	snprintf(scriptsBaseDir, sizeof(scriptsBaseDir), "%s/scripts/", exeDir);
	size_t baseLen = strlen(scriptsBaseDir);

	do {
		const char* filename = findData.cFileName;
		size_t namelen = strlen(filename);
		if (namelen < 3 || strcmp(filename + namelen - 3, ".sc") != 0)
			continue;

		char filepath[512];
		snprintf(filepath, sizeof(filepath), "%s%s", scriptsBaseDir, filename);
		
		FILE* f = fopen(filepath, "r");
		if (!f) continue;

		fseek(f, 0, SEEK_END);
		long fsize = ftell(f);
		fseek(f, 0, SEEK_SET);

		char* buffer = (char*)malloc(fsize + 1);
		fread(buffer, 1, fsize, f);
		buffer[fsize] = '\0';
		fclose(f);

		std::string scriptName = getScriptFilename(filepath);
		coordVars.clear();

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

	for (size_t i = 0; i < gEntities.size(); i++) {
		ScriptEntity& e = gEntities[i];

		bool shouldRender = false;
		switch (e.type) {
			case ENTITY_CAR: shouldRender = gRenderScriptCars; break;
			case ENTITY_PED: shouldRender = gRenderScriptPeds; break;
			case ENTITY_OBJECT: shouldRender = gRenderScriptObjects; break;
			case ENTITY_PICKUP: shouldRender = gRenderScriptPickups; break;
			case ENTITY_BLIP: shouldRender = gRenderScriptBlips; break;
			case ENTITY_COORD: shouldRender = gRenderScriptCoords; break;
		}

		if (!shouldRender) continue;

		float halfSize = 0.5f;
		float verticalOffset = 1.5f;

		rw::RGBA col;
		switch (e.type) {
			case ENTITY_CAR: col = { 0, 100, 255, 200 }; break;
			case ENTITY_PED: col = { 255, 200, 0, 200 }; break;
			case ENTITY_OBJECT: col = { 200, 100, 0, 200 }; break;
			case ENTITY_PICKUP: col = { 255, 0, 255, 200 }; break;
			case ENTITY_BLIP: col = { 255, 255, 0, 200 }; break;
			case ENTITY_COORD: col = { 150, 150, 150, 200 }; break;
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

		rw::V3d worldPos = { e.x, e.y, e.z + verticalOffset * 2.5f };
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
					case ENTITY_COORD: strncat(label, "[COORD] ", sizeof(label) - 1); break;
				}

				if (e.modelName[0]) {
					strncat(label, e.modelName, sizeof(label) - 1);
					strncat(label, " ", sizeof(label) - 1);
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
