#include "euryopa.h"
#include "audiozone.h"
#include "modloader.h"

static std::vector<AudioZoneEntry> audioZones;

bool AudioZones::gRenderAudioZones = false;

namespace AudioZones {

static void
ParseAudioZoneLine(const char *line)
{
	char name[32] = {0};
	int id = 0;
	int switchVal = 0;
	float coords[7] = {0};
	int numFloats = 0;
	int numInts = 0;

	char lineBuf[256];
	strncpy(lineBuf, line, 255);
	lineBuf[255] = '\0';

	char *token = strtok(lineBuf, " \t\n\r");
	while(token){
		if(!isdigit(token[0]) && !(token[0] == '-' && isdigit(token[1]))) {
			strncpy(name, token, 31);
		}else if(strchr(token, '.')){
			if(numFloats < 7) coords[numFloats] = atof(token);
			numFloats++;
		}else{
			int v = atoi(token);
			if(v != 0 || strcmp(token, "0") == 0){
				if(numInts == 0) id = v;
				else switchVal = v;
				numInts++;
			}
		}
		token = strtok(nil, " \t\n\r");
	}

	AudioZoneEntry entry;
	entry.id = id;
	entry.switchVal = switchVal;
	strncpy(entry.name, name, 31);

	if(numFloats == 6 && coords[3] != 0 && coords[4] != 0 && coords[5] != 0){
		entry.type = 1;
		entry.x1 = coords[0];
		entry.y1 = coords[1];
		entry.z1 = coords[2];
		entry.x2 = coords[3];
		entry.y2 = coords[4];
		entry.z2 = coords[5];
		audioZones.push_back(entry);
	}else if(numFloats == 4 && coords[3] > 0){
		entry.type = 2;
		entry.x1 = coords[0];
		entry.y1 = coords[1];
		entry.z1 = coords[2];
		entry.x2 = coords[3];
		entry.y2 = entry.z2 = 0;
		audioZones.push_back(entry);
	}
}

static void
LoadAudioZonesFromFile(const char *filepath)
{
	int size;
	uint8 *buf = ReadLooseFile(filepath, &size);
	if(buf == nil) return;

	char *auzoStart = strstr((char*)buf, "auzo");
	if(auzoStart == nil){ free(buf); return; }

	char *auzoEnd = strstr(auzoStart + 4, "end");
	if(auzoEnd == nil){ free(buf); return; }

	*auzoEnd = '\0';

	char *lineStart = auzoStart;
	char *p = auzoStart;
	while(*p){
		if(*p == '\n' || *p == '\r'){
			if(p > lineStart){
				*p = '\0';
				ParseAudioZoneLine(lineStart);
			}
			lineStart = p + 1;
		}
		p++;
	}
	if(p > lineStart){
		ParseAudioZoneLine(lineStart);
	}

	free(buf);
}

static void
ScanDirectory(const char *dir)
{
	char searchPath[256];
	snprintf(searchPath, sizeof(searchPath), "%s\\*.ipl", dir);

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPath, &findData);
	if(hFind == INVALID_HANDLE_VALUE) return;

	do{
		char filepath[256];
		snprintf(filepath, sizeof(filepath), "%s\\%s", dir, findData.cFileName);
		LoadAudioZonesFromFile(filepath);
	}while(FindNextFileA(hFind, &findData));

	FindClose(hFind);
}

static void
ScanSubdirectories(const char *dir)
{
	char searchPath[256];
	snprintf(searchPath, sizeof(searchPath), "%s\\*", dir);

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPath, &findData);
	if(hFind == INVALID_HANDLE_VALUE) return;

	do{
		if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
			if(findData.cFileName[0] != '.'){
				char subdir[256];
				snprintf(subdir, sizeof(subdir), "%s\\%s", dir, findData.cFileName);
				ScanDirectory(subdir);
				ScanSubdirectories(subdir);
			}
		}
	}while(FindNextFileA(hFind, &findData));

	FindClose(hFind);
}

void
Init(void)
{
	audioZones.clear();

	char exePath[260];
	GetModuleFileNameA(nil, exePath, sizeof(exePath));
	char *lastSlash = strrchr(exePath, '\\');
	if(lastSlash) *lastSlash = '\0';

	ScanDirectory(exePath);
	char dataPath[260];
	snprintf(dataPath, sizeof(dataPath), "%s\\data", exePath);
	ScanDirectory(dataPath);

	char mapsPath[260];
	snprintf(mapsPath, sizeof(mapsPath), "%s\\maps", exePath);
	ScanDirectory(mapsPath);
	ScanSubdirectories(mapsPath);

	log("AudioZones: loaded %d zones\n", audioZones.size());
}

bool
HasAudioZones(void)
{
	return !audioZones.empty();
}

void
Render(void)
{
	if(!gRenderAudioZones) return;

	uint8 alpha = 200;
	static const rw::RGBA col = { 255, 0, 255, alpha };

	for(size_t i = 0; i < audioZones.size(); i++){
		AudioZoneEntry &z = audioZones[i];

		if(z.type == 1){
			rw::V3d verts[8];
			verts[0] = { z.x1, z.y1, z.z1 };
			verts[1] = { z.x2, z.y1, z.z1 };
			verts[2] = { z.x2, z.y2, z.z1 };
			verts[3] = { z.x1, z.y2, z.z1 };
			verts[4] = { z.x1, z.y1, z.z2 };
			verts[5] = { z.x2, z.y1, z.z2 };
			verts[6] = { z.x2, z.y2, z.z2 };
			verts[7] = { z.x1, z.y2, z.z2 };

			RenderWireBoxVerts(verts, col);
		}else if(z.type == 2){
			CSphere sph;
			sph.center = { z.x1, z.y1, z.z1 };
			sph.radius = z.x2;
			RenderSphereAsWireBox(&sph, col, nil);
		}
	}
}

}