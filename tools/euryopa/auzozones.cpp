#include "euryopa.h"
#include "auzozones.h"
#include <vector>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

namespace AuzoZones
{

static std::vector<AuzoZone> zones;
bool gRenderAuzoZones = false;

void
Clear(void)
{
	zones.clear();
}

static void
LoadAuzoZoneData(const char *filename);

static void
LoadAuzoZoneLine(char *line);

static FileLoader::DatDesc auzoDesc[] = {
	{ "end", LoadNothing },
	{ "auzo", LoadAuzoZoneLine },
	{ "", nil }
};

void
LoadAuzoZoneLine(char *line)
{
	AuzoZone z;
	memset(&z, 0, sizeof(z));

	int count = 0;
	char *p = line;
	while(*p){
		if(*p == ',' || *p == ' ') count++;
		p++;
	}

	if(count >= 9){
		z.type = AuzoZone::BOX;
		int n = sscanf(line, "%31s %d %d %f %f %f %f %f %f %f",
			z.name, &z.soundId, &z.switchId,
			&z.box.x1, &z.box.y1, &z.box.z1,
			&z.box.x2, &z.box.y2, &z.box.z2);
		if(n >= 9){
			z.name[31] = '\0';
			zones.push_back(z);
		}
	}else if(count >= 7){
		z.type = AuzoZone::SPHERE;
		int n = sscanf(line, "%31s %d %d %f %f %f %f",
			z.name, &z.soundId, &z.switchId,
			&z.sphere.x, &z.sphere.y, &z.sphere.z,
			&z.sphere.radius);
		if(n >= 7){
			z.name[31] = '\0';
			zones.push_back(z);
		}
	}
}

static void
LoadAuzoZoneData(const char *filename)
{
	FILE *file;
	char *line;
	void (*handler)(char*) = nil;

	file = fopen_ci(filename, "rb");
	if(file == nil)
		return;

	while(line = FileLoader::LoadLine(file)){
		if(line[0] == '#')
			continue;
		void *tmp = FileLoader::DatDesc::get(auzoDesc, line);
		if(tmp){
			handler = (void(*)(char*))tmp;
			continue;
		}
		if(handler)
			handler(line);
	}
	fclose(file);
}

static void
ScanIplDirectory(const char *dir)
{
#ifdef _WIN32
	char searchPath[1024];
	WIN32_FIND_DATAA fd;
	snprintf(searchPath, sizeof(searchPath), "%s/*.ipl", dir);
	HANDLE hFind = FindFirstFileA(searchPath, &fd);
	if(hFind != INVALID_HANDLE_VALUE){
		do{
			if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
				char fullPath[1024];
				snprintf(fullPath, sizeof(fullPath), "%s/%s", dir, fd.cFileName);
				LoadAuzoZoneData(fullPath);
			}
		}while(FindNextFileA(hFind, &fd));
		FindClose(hFind);
	}

	snprintf(searchPath, sizeof(searchPath), "%s/*", dir);
	hFind = FindFirstFileA(searchPath, &fd);
	if(hFind != INVALID_HANDLE_VALUE){
		do{
			if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
				if(strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0){
					char subDir[1024];
					snprintf(subDir, sizeof(subDir), "%s/%s", dir, fd.cFileName);
					ScanIplDirectory(subDir);
				}
			}
		}while(FindNextFileA(hFind, &fd));
		FindClose(hFind);
	}
#else
	DIR *d = opendir(dir);
	if(d){
		struct dirent *ent;
		while((ent = readdir(d)) != nil){
			size_t len = strlen(ent->d_name);
			if(len > 4 && strcmp(ent->d_name + len - 4, ".ipl") == 0){
				char fullPath[1024];
				snprintf(fullPath, sizeof(fullPath), "%s/%s", dir, ent->d_name);
				LoadAuzoZoneData(fullPath);
			}
		}
		closedir(d);
	}
#endif
}

void
LoadAllAuzoZones(void)
{
	Clear();

	char gameRoot[1024];
	if(!GetGameRootDirectory(gameRoot, sizeof(gameRoot)))
		return;

	char fullPath[1024];

	snprintf(fullPath, sizeof(fullPath), "%s", gameRoot);
	ScanIplDirectory(fullPath);

	snprintf(fullPath, sizeof(fullPath), "%s/data", gameRoot);
	ScanIplDirectory(fullPath);

	snprintf(fullPath, sizeof(fullPath), "%s/data/maps", gameRoot);
	ScanIplDirectory(fullPath);
}

void
Render(void)
{
	if(!gRenderAuzoZones || zones.empty())
		return;

	rw::Matrix ident;
	ident.setIdentity();
	rw::RGBA col = { 255, 0, 255, 255 };

	for(size_t i = 0; i < zones.size(); i++){
		AuzoZone &z = zones[i];
		if(z.type == AuzoZone::BOX){
			CBox box;
			box.min.x = z.box.x1;
			box.min.y = z.box.y1;
			box.min.z = z.box.z1;
			box.max.x = z.box.x2;
			box.max.y = z.box.y2;
			box.max.z = z.box.z2;
			box.FindMinMax();
			RenderWireBox(&box, col, &ident);
		}else{
			CSphere sphere;
			sphere.center.x = z.sphere.x;
			sphere.center.y = z.sphere.y;
			sphere.center.z = z.sphere.z;
			sphere.radius = z.sphere.radius;
			RenderWireSphere(&sphere, col, &ident);
		}
	}
}

}