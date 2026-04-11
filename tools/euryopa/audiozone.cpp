#include "euryopa.h"
#include "audiozone.h"
#include "modloader.h"

#define MAXAUDIOZONES 1024
static AudioZoneEntry audioZones[MAXAUDIOZONES];
static int numAudioZones = 0;

bool AudioZones::gRenderAudioZones = false;

namespace AudioZones {

void
Init(void)
{
	numAudioZones = 0;

	log("AudioZones: searching for .ipl files in data/\n");

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA("data/*.ipl", &findData);
	if(hFind == INVALID_HANDLE_VALUE){
		log("AudioZones: no .ipl files in data/\n");
		return;
	}

	do{
		const char *filename = findData.cFileName;
		size_t namelen = strlen(filename);
		if(namelen < 4 || strcmp(filename + namelen - 4, ".ipl") != 0)
			continue;

		char filepath[256];
		snprintf(filepath, sizeof(filepath), "data/%s", filename);

		int size;
		uint8 *buf = ReadLooseFile(filepath, &size);
		if(buf == nil){
			log("AudioZones: failed to read %s\n", filepath);
			continue;
		}

		char *auzoStart = strstr((char*)buf, "auzo");
		if(auzoStart == nil){
			free(buf);
			continue;
		}

		char *auzoEnd = strstr(auzoStart + 4, "end");
		if(auzoEnd == nil){
			free(buf);
			continue;
		}

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
	}while(FindNextFileA(hFind, &findData));

	FindClose(hFind);

	log("AudioZones: loaded %d zones\n", numAudioZones);
}

void
ParseAudioZoneLine(const char *line)
{
	if(numAudioZones >= MAXAUDIOZONES) return;

	while(*line == ' ' || *line == '\t') line++;
	if(*line == '#' || *line == '\0') return;

	char name[32] = {0};
	int id = 0;
	int switchVal = 0;
	float coords[6] = {0};
	int numFloats = 0;
	int numInts = 0;

	char lineBuf[256];
	strncpy(lineBuf, line, sizeof(lineBuf)-1);

	char *token = strtok(lineBuf, " \t\n\r");
	while(token){
		if(!isdigit(token[0]) && !(token[0] == '-' && isdigit(token[1])){
			strncpy(name, token, 31);
		}else if(strchr(token, '.')){
			if(numFloats < 6) coords[numFloats] = atof(token);
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
		AudioZoneEntry &z = audioZones[numAudioZones];
		z.type = 1;
		strncpy(z.name, name, 31);
		z.id = id;
		z.switchVal = switchVal;
		z.box.min.x = coords[0];
		z.box.min.y = coords[1];
		z.box.min.z = coords[2];
		z.box.max.x = coords[3];
		z.box.max.y = coords[4];
		z.box.max.z = coords[5];
		z.box.FindMinMax();
		numAudioZones++;
	}
}

void
Render(void)
{
	if(!gRenderAudioZones)
		return;

	uint8 alpha = 180;
	static const rw::RGBA boxCol = { 255, 0, 255, alpha };

	for(int i = 0; i < numAudioZones; i++){
		AudioZoneEntry &z = audioZones[i];
		if(z.type == 1){
			RenderWireBox(&z.box, boxCol, nil);
		}
	}
}

bool
HasAudioZones(void)
{
	return numAudioZones > 0;
}

}