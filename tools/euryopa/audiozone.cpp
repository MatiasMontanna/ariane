#include "euryopa.h"
#include "audiozone.h"

namespace AudioZones
{

#define MAXAUDIOZONES 1024
static AudioZoneEntry gAudioZones[MAXAUDIOZONES];
static int gNumAudioZones = 0;

static int
TokenizeLine(const char *line, char tokens[16][64])
{
	int numTokens = 0;
	const char *p = line;
	int bi = 0;
	char buf[64];

	while(*p && numTokens < 16){
		if(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'){
			if(bi > 0){
				buf[bi] = '\0';
				strncpy(tokens[numTokens], buf, 63);
				tokens[numTokens][63] = '\0';
				numTokens++;
				bi = 0;
			}
		}else if(*p == '#'){
			break;
		}else{
			if(bi < 63) buf[bi++] = *p;
		}
		p++;
	}
	if(bi > 0 && numTokens < 16){
		buf[bi] = '\0';
		strncpy(tokens[numTokens], buf, 63);
		tokens[numTokens][63] = '\0';
		numTokens++;
	}
	return numTokens;
}

static bool
IsNumber(const char *s)
{
	if(*s == '-' || *s == '+') s++;
	while(*s){
		if(*s == '.') return true;
		if(*s < '0' || *s > '9') return false;
		s++;
	}
	return true;
}

static void
ParseAudioZoneLine(const char *line)
{
	if(gNumAudioZones >= MAXAUDIOZONES)
		return;

	char tokens[16][64];
	int numTokens = TokenizeLine(line, tokens);

	if(numTokens < 4)
		return;

	char name[32] = {0};
	int id = 0;
	int switchVal = 0;
	float x1=0, y1=0, z1=0, x2=0, y2=0, z2=0;
	float x=0, y=0, z=0, r=0;
	int type = 0;

	for(int i = 0; i < numTokens; i++){
		if(!IsNumber(tokens[i])){
			strncpy(name, tokens[i], 31);
		}
	}

	for(int i = 0; i < numTokens; i++){
		if(IsNumber(tokens[i])){
			float v = atof(tokens[i]);

			if(i == numTokens-6 && numTokens == 9){
				x1 = v;
			}else if(i == numTokens-5 && numTokens == 9){
				y1 = v;
			}else if(i == numTokens-4 && numTokens == 9){
				z1 = v;
			}else if(i == numTokens-3 && numTokens == 9){
				x2 = v;
			}else if(i == numTokens-2 && numTokens == 9){
				y2 = v;
			}else if(i == numTokens-1 && numTokens == 9){
				z2 = v;
			}else if(i == 3 && numTokens >= 7){
				x = v;
			}else if(i == 4 && numTokens >= 7){
				y = v;
			}else if(i == 5 && numTokens >= 7){
				z = v;
			}else if(i == 6 && numTokens >= 7){
				r = v;
				type = 2;
			}
		}
	}

	for(int i = 0; i < numTokens; i++){
		if(IsNumber(tokens[i])){
			int ival = atoi(tokens[i]);
			if(ival != 0 || (tokens[i][0] == '0' && tokens[i][1] == '\0')){
				if(id == 0) id = ival;
				else switchVal = ival;
			}
		}
	}

	if(numTokens == 9 && x2 != 0 && y2 != 0 && z2 != 0){
		AudioZoneEntry &z = gAudioZones[gNumAudioZones];
		z.type = 1;
		strncpy(z.name, name, 31);
		z.name[31] = '\0';
		z.id = id;
		z.switchVal = switchVal;
		z.box.min.x = x1;
		z.box.min.y = y1;
		z.box.min.z = z1;
		z.box.max.x = x2;
		z.box.max.y = y2;
		z.box.max.z = z2;
		z.box.FindMinMax();
		z.center = {0, 0, 0};
		z.radius = 0;
		gNumAudioZones++;
	}else if(type == 2 && r > 0){
		AudioZoneEntry &z = gAudioZones[gNumAudioZones];
		z.type = 2;
		strncpy(z.name, name, 31);
		z.name[31] = '\0';
		z.id = id;
		z.switchVal = switchVal;
		z.center = { x, y, z };
		z.radius = r;
		z.box.min = z.box.max = {0, 0, 0};
		gNumAudioZones++;
	}
}

static void
LoadAudioZonesFromFile(const char *path)
{
	FILE *f = fopen(path, "r");
	if(f == nil)
		return;

	char line[256];
	int inAuzo = 0;

	while(fgets(line, sizeof(line), f)){
		char *p = line;
		while(*p == ' ' || *p == '\t') p++;

		if(strncmp(p, "auzo", 4) == 0){
			inAuzo = 1;
			continue;
		}
		if(strncmp(p, "end", 3) == 0){
			inAuzo = 0;
			continue;
		}

		if(inAuzo){
			ParseAudioZoneLine(p);
		}
	}

	fclose(f);
}

void
LoadAllAudioZones(void)
{
	gNumAudioZones = 0;

	char exePath[MAXPATH];
	GetModuleFileNameA(nil, exePath, sizeof(exePath));
	char *lastSlash = strrchr(exePath, '\\');
	if(lastSlash) *lastSlash = '\0';

	char searchPath[MAXPATH];
	char iplPath[MAXPATH];
	WIN32_FIND_DATAA findData;
	HANDLE findHandle;

	snprintf(searchPath, sizeof(searchPath), "%s\\data", exePath);
	SetCurrentDirectoryA(searchPath);
	findHandle = FindFirstFileA("*.ipl", &findData);
	if(findHandle != INVALID_HANDLE_VALUE){
		do{
			snprintf(iplPath, sizeof(iplPath), "%s\\data\\%s", exePath, findData.cFileName);
			LoadAudioZonesFromFile(iplPath);
		}while(FindNextFileA(findHandle, &findData));
		FindClose(findHandle);
	}

	snprintf(searchPath, sizeof(searchPath), "%s\\maps", exePath);
	if(SetCurrentDirectoryA(searchPath)){
		findHandle = FindFirstFileA("*.ipl", &findData);
		if(findHandle != INVALID_HANDLE_VALUE){
			do{
				snprintf(iplPath, sizeof(iplPath), "%s\\maps\\%s", exePath, findData.cFileName);
				LoadAudioZonesFromFile(iplPath);
			}while(FindNextFileA(findHandle, &findData));
			FindClose(findHandle);
		}

		findHandle = FindFirstFileA("*", &findData);
		if(findHandle != INVALID_HANDLE_VALUE){
			do{
				if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
					if(findData.cFileName[0] != '.'){
						snprintf(searchPath, sizeof(searchPath), "%s\\maps\\%s", exePath, findData.cFileName);
						if(SetCurrentDirectoryA(searchPath)){
							HANDLE subHandle = FindFirstFileA("*.ipl", &findData);
							if(subHandle != INVALID_HANDLE_VALUE){
								do{
									snprintf(iplPath, sizeof(iplPath), "%s\\maps\\%s\\%s", exePath, findData.cFileName, findData.cFileName);
									LoadAudioZonesFromFile(iplPath);
								}while(FindNextFileA(subHandle, &findData));
								FindClose(subHandle);
							}
						}
					}
				}
			}while(FindNextFileA(findHandle, &findData));
			FindClose(findHandle);
		}
	}

	log("AudioZones: loaded %d zones\n", gNumAudioZones);
}

void
Render(void)
{
	if(!gRenderAudioZones)
		return;

	uint8 alpha = 180;
	static const rw::RGBA zoneCols[] = {
		{ 255, 0, 255, alpha },
		{ 0, 255, 255, alpha },
	};

	for(int i = 0; i < gNumAudioZones; i++){
		AudioZoneEntry &z = gAudioZones[i];
		rw::RGBA col = zoneCols[z.type - 1];

		if(z.type == 1){
			RenderWireBox(&z.box, col, nil);
		}else if(z.type == 2){
			CSphere sph;
			sph.center = z.center;
			sph.radius = z.radius;
			RenderSphereAsWireBox(&sph, col, nil);
		}
	}
}

}