#include "euryopa.h"
#include "carrec.h"
#include <vector>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <dirent.h>
#endif

static std::vector<CarrecRecording> recordings;

bool gRenderCarrec = false;
float gCarrecDrawDist = 500.0f;
int gSelectedCarrec = -1;
int gCarrecAnimationTime = 0;
bool gCarrecAnimate = false;

static const rw::RGBA carrecLineColor = { 255, 165, 0, 255 };
static const rw::RGBA carrecPointColor = { 255, 100, 0, 255 };
static const rw::RGBA carrecAnimColor = { 0, 255, 255, 255 };

namespace Carrec {

void
Init(const char *dataPath)
{
	recordings.clear();

	char searchPath[1024];
	char imgPath[1024];

	if(dataPath && dataPath[0]){
		snprintf(searchPath, sizeof(searchPath), "%s/carrec", dataPath);
		snprintf(imgPath, sizeof(imgPath), "%s/carrec/carrec.img", dataPath);
	}else{
		strncpy(searchPath, "carrec", sizeof(searchPath)-1);
		strncpy(imgPath, "carrec/carrec.img", sizeof(imgPath)-1);
	}
	searchPath[sizeof(searchPath)-1] = '\0';
	imgPath[sizeof(imgPath)-1] = '\0';

	FILE *img = fopen(imgPath, "rb");
	if(img){
		fseek(img, 0, SEEK_END);
		long imgSize = ftell(img);
		fseek(img, 0, SEEK_SET);

		int32 fourcc;
		fread(&fourcc, 4, 1, img);

		if(fourcc == 0x32524556){
			fseek(img, 0, SEEK_SET);
			fread(&fourcc, 4, 1, img);
			int32 numEntries;
			fread(&numEntries, 4, 1, img);

			for(int i = 0; i < numEntries; i++){
				char name[24] = {0};
				int32 offset;
				int32 size;
				fread(name, 24, 1, img);
				fread(&offset, 4, 1, img);
				fread(&size, 4, 1, img);

				if(size <= 0 || strstr(name, ".rrr") == nil)
					continue;

				int id = 0;
				if(sscanf(name, "carrec%d.rrr", &id) != 1)
					continue;

				CarrecRecording rec;
				rec.id = id;
				rec.points.clear();

				long savedPos = ftell(img);

				fseek(img, offset, SEEK_SET);

				int numPoints = size / 32;
				for(int p = 0; p < numPoints; p++){
					CarrecPoint pt;
					fread(&pt.time, 4, 1, img);
					fread(&pt.velX, 2, 1, img);
					fread(&pt.velY, 2, 1, img);
					fread(&pt.velZ, 2, 1, img);
					fread(&pt.rightX, 1, 1, img);
					fread(&pt.rightY, 1, 1, img);
					fread(&pt.rightZ, 1, 1, img);
					fread(&pt.topX, 1, 1, img);
					fread(&pt.topY, 1, 1, img);
					fread(&pt.topZ, 1, 1, img);
					fread(&pt.steering, 1, 1, img);
					fread(&pt.gas, 1, 1, img);
					fread(&pt.brake, 1, 1, img);
					fread(&pt.handbrake, 1, 1, img);
					fread(&pt.posX, 4, 1, img);
					fread(&pt.posY, 4, 1, img);
					fread(&pt.posZ, 4, 1, img);
					rec.points.push_back(pt);
				}

				fseek(img, savedPos, SEEK_SET);
				recordings.push_back(rec);
			}
		}else{
			char dirPath[1024];
			strncpy(dirPath, imgPath, sizeof(dirPath)-1);
			dirPath[sizeof(dirPath)-1] = '\0';
			char *ext = strrchr(dirPath, '.');
			if(ext) strcpy(ext, ".dir");

			FILE *dirFile = fopen(dirPath, "rb");
			if(dirFile){
				fseek(dirFile, 0, SEEK_END);
				int numEntries = ftell(dirFile) / 32;
				fseek(dirFile, 0, SEEK_SET);

				for(int i = 0; i < numEntries; i++){
					char name[24];
					int32 offset;
					int32 size;
					fread(name, 24, 1, dirFile);
					fread(&offset, 4, 1, dirFile);
					fread(&size, 4, 1, dirFile);

					if(size <= 0 || strstr(name, ".rrr") == nil)
						continue;

					int id = 0;
					if(sscanf(name, "carrec%d.rrr", &id) != 1)
						continue;

					CarrecRecording rec;
					rec.id = id;
					rec.points.clear();

					fseek(img, offset, SEEK_SET);

					int numPoints = size / 32;
					for(int p = 0; p < numPoints; p++){
						CarrecPoint pt;
						fread(&pt.time, 4, 1, img);
						fread(&pt.velX, 2, 1, img);
						fread(&pt.velY, 2, 1, img);
						fread(&pt.velZ, 2, 1, img);
						fread(&pt.rightX, 1, 1, img);
						fread(&pt.rightY, 1, 1, img);
						fread(&pt.rightZ, 1, 1, img);
						fread(&pt.topX, 1, 1, img);
						fread(&pt.topY, 1, 1, img);
						fread(&pt.topZ, 1, 1, img);
						fread(&pt.steering, 1, 1, img);
						fread(&pt.gas, 1, 1, img);
						fread(&pt.brake, 1, 1, img);
						fread(&pt.handbrake, 1, 1, img);
						fread(&pt.posX, 4, 1, img);
						fread(&pt.posY, 4, 1, img);
						fread(&pt.posZ, 4, 1, img);
						rec.points.push_back(pt);
					}

					recordings.push_back(rec);
				}
				fclose(dirFile);
			}
		}
		fclose(img);
	}else{
#ifdef _WIN32
		WIN32_FIND_DATA findData;
		HANDLE hFind = FindFirstFile(searchPath, &findData);
		if(hFind != INVALID_HANDLE_VALUE){
			FindClose(hFind);
		}

		char pattern[1024];
		snprintf(pattern, sizeof(pattern), "%s/*.rrr", searchPath);

		hFind = FindFirstFile(pattern, &findData);
		if(hFind != INVALID_HANDLE_VALUE){
			do {
				if(findData.cFileName[0] == '.')
					continue;

				int id = 0;
				if(sscanf(findData.cFileName, "carrec%d.rrr", &id) != 1)
					continue;

				char rrrPath[1024];
				snprintf(rrrPath, sizeof(rrrPath), "%s/%s", searchPath, findData.cFileName);

				FILE *f = fopen(rrrPath, "rb");
				if(f == nil)
					continue;

				CarrecRecording rec;
				rec.id = id;
				rec.points.clear();

				fseek(f, 0, SEEK_END);
				int fileSize = ftell(f);
				fseek(f, 0, SEEK_SET);

				int numPoints = fileSize / 32;
				for(int p = 0; p < numPoints; p++){
					CarrecPoint pt;
					fread(&pt.time, 4, 1, f);
					fread(&pt.velX, 2, 1, f);
					fread(&pt.velY, 2, 1, f);
					fread(&pt.velZ, 2, 1, f);
					fread(&pt.rightX, 1, 1, f);
					fread(&pt.rightY, 1, 1, f);
					fread(&pt.rightZ, 1, 1, f);
					fread(&pt.topX, 1, 1, f);
					fread(&pt.topY, 1, 1, f);
					fread(&pt.topZ, 1, 1, f);
					fread(&pt.steering, 1, 1, f);
					fread(&pt.gas, 1, 1, f);
					fread(&pt.brake, 1, 1, f);
					fread(&pt.handbrake, 1, 1, f);
					fread(&pt.posX, 4, 1, f);
					fread(&pt.posY, 4, 1, f);
					fread(&pt.posZ, 4, 1, f);
					rec.points.push_back(pt);
				}
				fclose(f);
				recordings.push_back(rec);
			} while(FindNextFile(hFind, &findData));
			FindClose(hFind);
		}
#else
		DIR *dir = opendir(searchPath);
		if(dir){
			struct dirent *entry;
			while((entry = readdir(dir)) != nil){
				if(entry->d_name[0] == '.')
					continue;
				if(strstr(entry->d_name, ".rrr") == nil)
					continue;

				int id = 0;
				if(sscanf(entry->d_name, "carrec%d.rrr", &id) != 1)
					continue;

				char rrrPath[1024];
				snprintf(rrrPath, sizeof(rrrPath), "%s/%s", searchPath, entry->d_name);

				FILE *f = fopen(rrrPath, "rb");
				if(f == nil)
					continue;

				CarrecRecording rec;
				rec.id = id;
				rec.points.clear();

				fseek(f, 0, SEEK_END);
				int fileSize = ftell(f);
				fseek(f, 0, SEEK_SET);

				int numPoints = fileSize / 32;
				for(int p = 0; p < numPoints; p++){
					CarrecPoint pt;
					fread(&pt.time, 4, 1, f);
					fread(&pt.velX, 2, 1, f);
					fread(&pt.velY, 2, 1, f);
					fread(&pt.velZ, 2, 1, f);
					fread(&pt.rightX, 1, 1, f);
					fread(&pt.rightY, 1, 1, f);
					fread(&pt.rightZ, 1, 1, f);
					fread(&pt.topX, 1, 1, f);
					fread(&pt.topY, 1, 1, f);
					fread(&pt.topZ, 1, 1, f);
					fread(&pt.steering, 1, 1, f);
					fread(&pt.gas, 1, 1, f);
					fread(&pt.brake, 1, 1, f);
					fread(&pt.handbrake, 1, 1, f);
					fread(&pt.posX, 4, 1, f);
					fread(&pt.posY, 4, 1, f);
					fread(&pt.posZ, 4, 1, f);
					rec.points.push_back(pt);
				}
				fclose(f);
				recordings.push_back(rec);
			}
			closedir(dir);
		}
#endif
	}

	log("Carrec: loaded %d recordings\n", (int)recordings.size());
}

void
Shutdown(void)
{
	recordings.clear();
}

int
GetNumRecordings(void)
{
	return recordings.size();
}

CarrecRecording*
GetRecording(int idx)
{
	if(idx < 0 || idx >= (int)recordings.size())
		return nil;
	return &recordings[idx];
}

void
Render(void)
{
	if(!gRenderCarrec)
		return;

	for(size_t i = 0; i < recordings.size(); i++){
		CarrecRecording &rec = recordings[i];
		if(rec.points.empty())
			continue;

		rw::V3d prevPos = { rec.points[0].posX, rec.points[0].posY, rec.points[0].posZ + 1.0f };

		for(size_t p = 1; p < rec.points.size(); p++){
			CarrecPoint &pt = rec.points[p];
			rw::V3d pos = { pt.posX, pt.posY, pt.posZ + 1.0f };

			if(TheCamera.distanceTo(pos) > gCarrecDrawDist)
				continue;

			rw::RGBA col = carrecLineColor;
			if(gSelectedCarrec == rec.id)
				col = carrecAnimColor;

			RenderLine(prevPos.x, prevPos.y, prevPos.z,
			           pos.x, pos.y, pos.z, col, col);

			prevPos = pos;
		}

		if(rec.points.size() > 0 && rec.points.size() <= 100){
			CSphere sphere;
			for(size_t p = 0; p < rec.points.size(); p++){
				CarrecPoint &pt = rec.points[p];
				sphere.center.x = pt.posX;
				sphere.center.y = pt.posY;
				sphere.center.z = pt.posZ + 1.0f;
				sphere.radius = 0.3f;

				if(TheCamera.distanceTo(sphere.center) <= gCarrecDrawDist)
					RenderWireSphere(&sphere, carrecPointColor, nil);
			}
		}
	}

	if(gCarrecAnimate && gSelectedCarrec >= 0){
		for(size_t i = 0; i < recordings.size(); i++){
			CarrecRecording &rec = recordings[i];
			if(rec.id != gSelectedCarrec)
				continue;
			if(rec.points.empty())
				continue;

			int animTime = gCarrecAnimationTime;
			CarrecPoint *prevPt = nil;
			CarrecPoint *nextPt = nil;

			for(size_t p = 0; p < rec.points.size(); p++){
				if(rec.points[p].time <= animTime){
					prevPt = &rec.points[p];
				}
				if(rec.points[p].time > animTime && nextPt == nil){
					nextPt = &rec.points[p];
					break;
				}
			}

			if(prevPt){
				rw::V3d pos = { prevPt->posX, prevPt->posY, prevPt->posZ + 1.0f };

				CSphere sphere;
				sphere.center = pos;
				sphere.radius = 2.0f;
				RenderSphereAsCross(&sphere, carrecAnimColor, nil);

				rw::V3d right = {
					(float)prevPt->rightX / 127.0f,
					(float)prevPt->rightY / 127.0f,
					(float)prevPt->rightZ / 127.0f
				};
				rw::V3d top = {
					(float)prevPt->topX / 127.0f,
					(float)prevPt->topY / 127.0f,
					(float)prevPt->topZ / 127.0f
				};

				rw::V3d forward = cross(right, top);

				rw::V3d carFront = add(pos, scale(forward, 5.0f));
				RenderLine(pos.x, pos.y, pos.z, carFront.x, carFront.y, carFront.z,
					carrecAnimColor, carrecAnimColor);

				rw::V3d carRight = add(pos, scale(right, 2.0f));
				RenderLine(pos.x, pos.y, pos.z, carRight.x, carRight.y, carRight.z,
					{ 255, 0, 0, 255 }, { 255, 0, 0, 255 });

				rw::V3d carUp = add(pos, scale(top, 2.0f));
				RenderLine(pos.x, pos.y, pos.z, carUp.x, carUp.y, carUp.z,
					{ 0, 255, 0, 255 }, { 0, 255, 0, 255 });
			}
		}
	}
}

}
