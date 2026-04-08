#include "euryopa.h"
#include "carrec.h"
#include "modloader.h"

static std::vector<CarrecPath> carrecPaths;

static void
CarrecLog(const char *msg)
{
	char logpath[256];
	snprintf(logpath, sizeof(logpath), "carrec_debug.txt");
	FILE *f = fopen(logpath, "a");
	if(f){
		fprintf(f, "%s\n", msg);
		fclose(f);
	}
}

namespace Carrec {

void
Init(void)
{
	char path[256];
	snprintf(path, sizeof(path), "data/Paths/carrec.img");
	CarrecLog("Carrec: Starting...");

	int size;
	uint8 *buf = ReadLooseFile(path, &size);
	if(buf == nil){
		CarrecLog("Carrec: carrec.img not found");
		log("Carrec: carrec.img not found at %s\n", path);
		return;
	}

	if(size < 8){
		CarrecLog("Carrec: carrec.img too small");
		free(buf);
		return;
	}

	uint32 magic = *(uint32*)buf;
	if(magic != 0x32524556){  // "VER2"
		CarrecLog("Carrec: not VER2 format");
		log("Carrec: carrec.img is not VER2 format\n");
		free(buf);
		return;
	}

	int numFiles = *(int*)(buf + 4);
	char tmp[256];
	snprintf(tmp, sizeof(tmp), "Carrec: found %d files", numFiles);
	CarrecLog(tmp);
	log("Carrec: found %d files in carrec.img\n", numFiles);

	log("Carrec: trying to load .rrr files\n");

	uint8 *ptr = buf + 8;
	for(int i = 0; i < numFiles; i++){
		char name[24];
		uint32 offset = *(uint32*)(ptr + 0);
		uint16 size = *(uint16*)(ptr + 6);  // sizeInArchive at offset 6
		memcpy(name, ptr + 8, 24);
		name[23] = '\0';

		// Skip non-.rrr files
		size_t namelen = strlen(name);
		if(namelen < 4 || strcmp(name + namelen - 4, ".rrr") != 0){
			ptr += 32;
			continue;
		}

		snprintf(tmp, sizeof(tmp), "Carrec: .rrr file: %s offset=%d size=%d", name, offset, size);
		CarrecLog(tmp);

		// Skip files that are too small (less than 32 bytes = 1 node)
		if(size < 32){
			snprintf(tmp, sizeof(tmp), "Carrec: %s too small, skipping", name);
			CarrecLog(tmp);
			ptr += 32;
			continue;
		}

		uint8 *fileData = buf + offset;
		int numNodes = size / 32;

		snprintf(tmp, sizeof(tmp), "Carrec: %s has %d nodes", name, numNodes);
		CarrecLog(tmp);

		CarrecPath pathData;
		strncpy(pathData.name, name, sizeof(pathData.name) - 1);
		pathData.name[sizeof(pathData.name) - 1] = '\0';
		pathData.nodes.resize(numNodes);

		for(int j = 0; j < numNodes; j++){
			uint8 *nodeData = fileData + j * 32;
			CarrecNode &node = pathData.nodes[j];
			node.time = *(float*)(nodeData + 0);
			node.velocityX = *(float*)(nodeData + 4);
			node.velocityY = *(float*)(nodeData + 8);
			node.velocityZ = *(float*)(nodeData + 12);
			node.orientW = *(int16*)(nodeData + 16);
			node.orientX = *(int16*)(nodeData + 18);
			node.orientY = *(int16*)(nodeData + 20);
			node.orientZ = *(int16*)(nodeData + 22);
			node.steering = *(int16*)(nodeData + 24);
			node.gas = *(uint16*)(nodeData + 26);
			node.brake = *(uint16*)(nodeData + 28);
			node.posX = *(float*)(nodeData + 24);
			node.posY = *(float*)(nodeData + 28);
			node.posZ = *(float*)(nodeData + 32);
		}

		carrecPaths.push_back(pathData);
		ptr += 32;
	}

	free(buf);
	snprintf(tmp, sizeof(tmp), "Carrec: loaded %d paths", (int)carrecPaths.size());
	CarrecLog(tmp);
	log("Carrec: loaded %d paths\n", carrecPaths.size());
}

void
Render(void)
{
	if(carrecPaths.empty())
		return;

	uint8 alpha = (uint8)(gCollisionWireframeAlpha * 255.0f);
	rw::RGBA col = { 255, 165, 0, alpha };  // Orange

	for(size_t i = 0; i < carrecPaths.size(); i++){
		CarrecPath &path = carrecPaths[i];
		if(path.nodes.size() < 2)
			continue;

		for(size_t j = 0; j < path.nodes.size() - 1; j++){
			rw::V3d v1 = { path.nodes[j].posX, path.nodes[j].posY, path.nodes[j].posZ };
			rw::V3d v2 = { path.nodes[j+1].posX, path.nodes[j+1].posY, path.nodes[j+1].posZ };
			RenderLine(v1, v2, col, col);
		}
	}
}

}  // namespace Carrec