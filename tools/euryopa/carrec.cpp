#include "euryopa.h"
#include "carrec.h"
#include "modloader.h"

static std::vector<CarrecPath> carrecPaths;

bool Carrec::gRenderAsLines = true;
bool Carrec::gRenderAsCubes = false;

static void
CarrecLog(const char *msg)
{
	FILE *f = fopen("carrec_debug.txt", "a");
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
	if(magic != 0x32524556){
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
		uint32 offsetSectors = *(uint32*)(ptr + 0);
		uint16 streamingSizeSectors = *(uint16*)(ptr + 4);
		uint16 sizeInArchiveSectors = *(uint16*)(ptr + 6);
		memcpy(name, ptr + 8, 24);
		name[23] = '\0';

		uint32 offset = offsetSectors * 2048;
		uint32 size = (streamingSizeSectors > 0 ? streamingSizeSectors : sizeInArchiveSectors) * 2048;

		size_t namelen = strlen(name);
		if(namelen < 4 || strcmp(name + namelen - 4, ".rrr") != 0){
			ptr += 32;
			continue;
		}

		snprintf(tmp, sizeof(tmp), "Carrec: .rrr file: %s offsetSectors=%d offsetBytes=%d sizeSectors=%d sizeBytes=%d", 
			name, offsetSectors, offset, sizeInArchiveSectors, size);
		CarrecLog(tmp);

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
		pathData.enabled = true;

		for(int j = 0; j < numNodes; j++){
			uint8 *nodeData = fileData + j * 32;
			CarrecNode &node = pathData.nodes[j];

			node.time = *(int32*)(nodeData + 0);
			node.posX = *(float*)(nodeData + 20);
			node.posY = *(float*)(nodeData + 24);
			node.posZ = *(float*)(nodeData + 28);

			if(j == 0){
				snprintf(tmp, sizeof(tmp), "Carrec: first node time=%d pos=(%.2f, %.2f, %.2f)",
					node.time, node.posX, node.posY, node.posZ);
				CarrecLog(tmp);

				snprintf(tmp, sizeof(tmp), "Carrec: raw bytes at 20-31: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
					nodeData[20], nodeData[21], nodeData[22], nodeData[23],
					nodeData[24], nodeData[25], nodeData[26], nodeData[27],
					nodeData[28], nodeData[29], nodeData[30], nodeData[31]);
				CarrecLog(tmp);
			}
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
	rw::RGBA col = { 255, 165, 0, alpha };

	for(size_t i = 0; i < carrecPaths.size(); i++){
		CarrecPath &path = carrecPaths[i];
		if(path.nodes.empty() || !path.enabled)
			continue;

		if(Carrec::gRenderAsLines && path.nodes.size() >= 2){
			for(size_t j = 0; j < path.nodes.size() - 1; j++){
				rw::V3d v1 = { path.nodes[j].posX, path.nodes[j].posY, path.nodes[j].posZ };
				rw::V3d v2 = { path.nodes[j+1].posX, path.nodes[j+1].posY, path.nodes[j+1].posZ };
				RenderLine(v1, v2, col, col);
			}
		}

		if(Carrec::gRenderAsCubes){
			float half = 0.5f;
			for(size_t j = 0; j < path.nodes.size(); j++){
				rw::V3d p = { path.nodes[j].posX, path.nodes[j].posY, path.nodes[j].posZ };
				rw::V3d v[8] = {
					{ p.x - half, p.y - half, p.z - half },
					{ p.x + half, p.y - half, p.z - half },
					{ p.x - half, p.y + half, p.z - half },
					{ p.x + half, p.y + half, p.z - half },
					{ p.x - half, p.y - half, p.z + half },
					{ p.x + half, p.y - half, p.z + half },
					{ p.x - half, p.y + half, p.z + half },
					{ p.x + half, p.y + half, p.z + half }
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
			}
		}
	}
}

int
GetNumPaths(void)
{
	return (int)carrecPaths.size();
}

CarrecPath *
GetPath(int index)
{
	if(index >= 0 && index < (int)carrecPaths.size())
		return &carrecPaths[index];
	return nil;
}

void
SetAllPaths(bool enabled)
{
	for(size_t i = 0; i < carrecPaths.size(); i++)
		carrecPaths[i].enabled = enabled;
}

}  // namespace Carrec