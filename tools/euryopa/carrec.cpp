#include "euryopa.h"
#include "carrec.h"
#include <vector>
#include <cstring>

namespace Carrec {

static std::vector<CarrecPath> gPaths;
static bool gVisible = false;

struct RRREntry {
	char name[24];
	uint32 offset;
	uint32 size;
};

bool
LoadCarrecIMG(void)
{
	char path[512];
	if(!GetGameRootDirectory(path, sizeof(path)))
		return false;

	strcat(path, "data\\Paths\\carrec.img");

	FILE *f = fopen(path, "rb");
	if(f == nil){
		log("Carrec: failed to open %s\n", path);
		return false;
	}

	char fourcc[4];
	fread(fourcc, 1, 4, f);
	if(strncmp(fourcc, "IMG$", 4) != 0){
		log("Carrec: invalid IMG format\n");
		fclose(f);
		return false;
	}

	uint32 numEntries;
	fread(&numEntries, 4, 1, f);

	log("Carrec: loading %d entries from %s\n", numEntries, path);

	gPaths.clear();
	gPaths.reserve(numEntries);

	for(uint32 i = 0; i < numEntries; i++){
		RRREntry entry;
		fread(&entry, sizeof(entry), 1, f);

		char *name = entry.name;
		size_t namelen = strlen(name);
		if(namelen > 4 && strcmp(name + namelen - 4, ".rrr") == 0){
			fseek(f, entry.offset * 2048, SEEK_SET);

			uint32 numPoints = entry.size / 32;
			CarrecPath pathData;
			pathData.name = name;
			pathData.points.resize(numPoints);

			for(uint32 j = 0; j < numPoints; j++){
				CarrecPoint &pt = pathData.points[j];
				fread(&pt, 32, 1, f);
			}

			gPaths.push_back(pathData);
			log("Carrec: loaded %s with %d points\n", name, numPoints);
		}
	}

	fclose(f);
	log("Carrec: loaded %d paths\n", gPaths.size());
	return gPaths.size() > 0;
}

void
Load(void)
{
	if(!LoadCarrecIMG()){
		log("Carrec: no carrec.img found or failed to load\n");
	}
}

static const rw::RGBA pathColor = { 255, 255, 0, 255 };

void
Render(void)
{
	if(!gVisible || gPaths.empty())
		return;

	float drawDist = 300.0f;

	for(size_t p = 0; p < gPaths.size(); p++){
		CarrecPath &path = gPaths[p];

		for(size_t i = 0; i + 1 < path.points.size(); i++){
			CarrecPoint &p1 = path.points[i];
			CarrecPoint &p2 = path.points[i + 1];

			rw::V3d pos1 = { p1.posX, p1.posY, p1.posZ + 1.0f };
			rw::V3d pos2 = { p2.posX, p2.posY, p2.posZ + 1.0f };

			if(TheCamera.distanceTo(pos1) > drawDist &&
			   TheCamera.distanceTo(pos2) > drawDist)
				continue;

			RenderLine(pos1, pos2, pathColor, pathColor);
		}
	}
}

int
GetNumPaths(void)
{
	return gPaths.size();
}

const char *
GetPathName(int i)
{
	if(i < 0 || (size_t)i >= gPaths.size())
		return "";
	return gPaths[i].name.c_str();
}

void
SetVisible(bool visible)
{
	gVisible = visible;
}

bool
IsVisible(void)
{
	return gVisible;
}

}