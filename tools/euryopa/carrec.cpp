#include "euryopa.h"
#include "carrec.h"

#include <vector>
#include <cstring>

static std::vector<CarrecPath> carrecPaths;
static bool carrecInitialised = false;

namespace Carrec {

void
Init(void)
{
	if(carrecInitialised)
		return;

	char carrecImgPath[256];
	snprintf(carrecImgPath, sizeof(carrecImgPath), "%sdata/Paths/carrec.img", getPath(""));

	FILE *f = fopen(carrecImgPath, "rb");
	if(f == nil){
		log("Carrec: could not open carrec.img at %s\n", carrecImgPath);
		return;
	}

	uint32 fourcc;
	fread(&fourcc, 1, 4, f);
	if(fourcc != 0x32524556){	// "VER2"
		fclose(f);
		log("Carrec: carrec.img is not VER2 format\n");
		return;
	}

	int numFiles;
	fread(&numFiles, 1, 4, f);

	for(int i = 0; i < numFiles; i++){
		CarrecPath path;
		memset(&path, 0, sizeof(path));

		char name[25];
		memset(name, 0, sizeof(name));
		fread(name, 1, 24, f);
		strncpy(path.name, name, 24);
		path.name[24] = '\0';

		int numNodes;
		fread(&numNodes, 1, 4, f);

		if(numNodes > 0){
			path.numNodes = numNodes;
			path.nodes = (CarrecNode*)malloc(numNodes * sizeof(CarrecNode));
			fread(path.nodes, sizeof(CarrecNode), numNodes, f);
		}else{
			path.nodes = nil;
		}

		carrecPaths.push_back(path);
	}

	fclose(f);
	carrecInitialised = true;
	log("Carrec: loaded %d paths\n", carrecPaths.size());
}

void
Shutdown(void)
{
	for(size_t i = 0; i < carrecPaths.size(); i++){
		if(carrecPaths[i].nodes)
			free(carrecPaths[i].nodes);
	}
	carrecPaths.clear();
	carrecInitialised = false;
}

void
Render(void)
{
	if(!carrecInitialised || carrecPaths.empty())
		return;

	static const rw::RGBA carrecColor = { 255, 165, 0, 255 };

	for(size_t i = 0; i < carrecPaths.size(); i++){
		CarrecPath &p = carrecPaths[i];
		if(p.numNodes < 2 || p.nodes == nil)
			continue;

		for(int j = 0; j < p.numNodes - 1; j++){
			rw::V3d p1 = { p.nodes[j].posX, p.nodes[j].posY, p.nodes[j].posZ };
			rw::V3d p2 = { p.nodes[j+1].posX, p.nodes[j+1].posY, p.nodes[j+1].posZ };

			if(TheCamera.distanceTo(p1) > 300.0f)
				continue;

			RenderLine(p1, p2, carrecColor, carrecColor);
		}
	}
}

int
GetNumPaths(void)
{
	return carrecPaths.size();
}

const char *
GetPathName(int idx)
{
	if(idx < 0 || idx >= (int)carrecPaths.size())
		return "";
	return carrecPaths[idx].name;
}

}