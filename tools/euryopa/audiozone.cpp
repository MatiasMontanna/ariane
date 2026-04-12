#include "euryopa.h"
#include "audiozone.h"
#include "modloader.h"

#define MAXAUDIOZONES 1024
static AudioZoneEntry gAudioZones[MAXAUDIOZONES];
static int gNumAudioZones = 0;

bool AudioZones::gRenderAudioZones = false;

static void
ParseAudioZoneLine(const char *line)
{
	if(gNumAudioZones >= MAXAUDIOZONES) return;

	while(*line == ' ' || *line == '\t') line++;
	if(*line == '#' || *line == '\0') return;

	char name[32] = {0};
	int id = 0;
	int switchVal = 0;
	float coords[7] = {0};
	int numFloats = 0;
	int numInts = 0;

	char lineBuf[256];
	strncpy(lineBuf, line, 255);

	char *token = strtok(lineBuf, " \t\n\r");
	while(token){
		if(!isdigit(token[0]) && !(token[0] == '-' && isdigit(token[1])){
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

	if(numFloats == 6 && coords[3] != 0 && coords[4] != 0 && coords[5] != 0){
		AudioZoneEntry &z = gAudioZones[gNumAudioZones];
		z.type = 1;
		strncpy(z.name, name, 31);
		z.id = id;
		z.switchVal = switchVal;
		z.boxMinX = coords[0];
		z.boxMinY = coords[1];
		z.boxMinZ = coords[2];
		z.boxMaxX = coords[3];
		z.boxMaxY = coords[4];
		z.boxMaxZ = coords[5];
		z.center = {0, 0, 0};
		z.radius = 0;
		gNumAudioZones++;
	}else if(numFloats == 4 && coords[3] > 0){
		AudioZoneEntry &z = gAudioZones[gNumAudioZones];
		z.type = 2;
		strncpy(z.name, name, 31);
		z.id = id;
		z.switchVal = switchVal;
		z.center = { coords[0], coords[1], coords[2] };
		z.radius = coords[3];
		z.boxMinX = z.boxMaxX = 0;
		z.boxMinY = z.boxMaxY = 0;
		z.boxMinZ = z.boxMaxZ = 0;
		gNumAudioZones++;
	}
}

static void
LoadAudioZonesFromFile(const char *path)
{
	int size;
	uint8 *buf = ReadLooseFile(path, &size);
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
ScanAudioZonesDirectory(const char *dir)
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
ScanAudioZonesSubdirectories(const char *dir)
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
				ScanAudioZonesDirectory(subdir);
			}
		}
	}while(FindNextFileA(hFind, &findData));

	FindClose(hFind);
}

void
AudioZones::Init(void)
{
	gNumAudioZones = 0;

	char exePath[MAXPATH];
	GetModuleFileNameA(nil, exePath, sizeof(exePath));
	char *lastSlash = strrchr(exePath, '\\');
	if(lastSlash) *lastSlash = '\0';

	ScanAudioZonesDirectory(exePath);
	char dataPath[MAXPATH];
	snprintf(dataPath, sizeof(dataPath), "%s\\data", exePath);
	ScanAudioZonesDirectory(dataPath);

	char mapsPath[MAXPATH];
	snprintf(mapsPath, sizeof(mapsPath), "%s\\maps", exePath);
	ScanAudioZonesDirectory(mapsPath);
	ScanAudioZonesSubdirectories(mapsPath);

	log("AudioZones: loaded %d zones\n", gNumAudioZones);
}

bool
AudioZones::HasAudioZones(void)
{
	return gNumAudioZones > 0;
}

void
AudioZones::Render(void)
{
	if(!gRenderAudioZones) return;

	uint8 alpha = 180;
	static const rw::RGBA col = { 255, 0, 255, alpha };

	for(int i = 0; i < gNumAudioZones; i++){
		AudioZoneEntry &z = gAudioZones[i];

		if(z.type == 1){
			rw::V3d verts[8];
			verts[0] = { z.boxMinX, z.boxMinY, z.boxMinZ };
			verts[1] = { z.boxMaxX, z.boxMinY, z.boxMinZ };
			verts[2] = { z.boxMaxX, z.boxMaxY, z.boxMinZ };
			verts[3] = { z.boxMinX, z.boxMaxY, z.boxMinZ };
			verts[4] = { z.boxMinX, z.boxMinY, z.boxMaxZ };
			verts[5] = { z.boxMaxX, z.boxMinY, z.boxMaxZ };
			verts[6] = { z.boxMaxX, z.boxMaxY, z.boxMaxZ };
			verts[7] = { z.boxMinX, z.boxMaxY, z.boxMaxZ };

			RenderWireBoxVerts(verts, col);
		}else if(z.type == 2){
			CSphere sph;
			sph.center = z.center;
			sph.radius = z.radius;
			RenderSphereAsWireBox(&sph, col, nil);
		}
	}
}