#include "euryopa.h"
#include "carrec.h"
#include <cstdio>
#include <cstring>

enum ExportFormat { EXPORT_JSON, EXPORT_CSV };

static const char*
getZoneTypeName(int type)
{
	switch(type){
	case 0: return "Navig0";
	case 1: return "Navig1";
	case 2: return "Info";
	case 3: return "MapZone";
	default: return "Unknown";
	}
}

static const char*
getEffectTypeName(int type)
{
	switch(type){
	case FX_LIGHT: return "Light";
	case FX_PARTICLE: return "Particle";
	case FX_LOOKATPOINT: return "LookAtPoint";
	case FX_PEDQUEUE: return "PedQueue";
	case FX_INTERIOR: return "Interior";
	case FX_ENTRYEXIT: return "EntryExit";
	case FX_ROADSIGN: return "Roadsign";
	case FX_TRIGGERPOINT: return "TriggerPoint";
	case FX_COVERPOINT: return "CoverPoint";
	case FX_ESCALATOR: return "Escalator";
	default: return "Unknown";
	}
}

static void
ExportObjectsJSON(FILE *file)
{
	fprintf(file, "[\n");
	bool first = true;
	for(CPtrNode *node = instances.first; node != nil; node = node->next){
		ObjectInst *inst = (ObjectInst*)node->item;
		if(inst->m_isDeleted)
			continue;

		ObjectDef *obj = GetObjectDef(inst->m_objectId);

		if(!first) fprintf(file, ",\n");
		first = false;

		fprintf(file, "  {\n");
		fprintf(file, "    \"instanceId\": %d,\n", inst->m_id);
		fprintf(file, "    \"objectId\": %d,\n", inst->m_objectId);
		if(obj){
			fprintf(file, "    \"objectName\": \"%s\",\n", obj->m_name);
			fprintf(file, "    \"drawDist\": [%.4f, %.4f, %.4f],\n",
				obj->m_drawDist[0], obj->m_drawDist[1], obj->m_drawDist[2]);
			fprintf(file, "    \"minDrawDist\": %.4f,\n", obj->m_minDrawDist);
			fprintf(file, "    \"flags\": {\n");
			fprintf(file, "      \"drawLast\": %s,\n", obj->m_drawLast ? "true" : "false");
			fprintf(file, "      \"additive\": %s,\n", obj->m_additive ? "true" : "false");
			fprintf(file, "      \"noFade\": %s,\n", obj->m_noFade ? "true" : "false");
			fprintf(file, "      \"noZwrite\": %s,\n", obj->m_noZwrite ? "true" : "false");
			fprintf(file, "      \"noShadows\": %s,\n", obj->m_noShadows ? "true" : "false");
			fprintf(file, "      \"ignoreDrawDist\": %s,\n", obj->m_ignoreDrawDist ? "true" : "false");
			fprintf(file, "      \"isCodeGlass\": %s,\n", obj->m_isCodeGlass ? "true" : "false");
			fprintf(file, "      \"isArtistGlass\": %s,\n", obj->m_isArtistGlass ? "true" : "false");
			fprintf(file, "      \"noBackfaceCulling\": %s,\n", obj->m_noBackfaceCulling ? "true" : "false");
			fprintf(file, "      \"isGarageDoor\": %s,\n", obj->m_isGarageDoor ? "true" : "false");
			fprintf(file, "      \"isDamageable\": %s,\n", obj->m_isDamageable ? "true" : "false");
			fprintf(file, "      \"isTree\": %s,\n", obj->m_isTree ? "true" : "false");
			fprintf(file, "      \"isPalmTree\": %s,\n", obj->m_isPalmTree ? "true" : "false");
			fprintf(file, "      \"isDoor\": %s,\n", obj->m_isDoor ? "true" : "false");
			fprintf(file, "      \"isTimed\": %s\n", obj->m_isTimed ? "true" : "false");
			fprintf(file, "    }");
			if(obj->m_isTimed){
				fprintf(file, ",\n    \"timeOn\": %d, \"timeOff\": %d", obj->m_timeOn, obj->m_timeOff);
			}
		}
		fprintf(file, ",\n    \"position\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
			inst->m_translation.x, inst->m_translation.y, inst->m_translation.z);
		fprintf(file, "    \"rotation\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f, \"w\": %.4f },\n",
			inst->m_rotation.x, inst->m_rotation.y,
			inst->m_rotation.z, inst->m_rotation.w);
		fprintf(file, "    \"area\": %d,\n", inst->m_area);
		if(obj && obj->m_numEffects > 0){
			fprintf(file, "    \"numEffects\": %d,\n", obj->m_numEffects);
		}
		if(obj && obj->m_carPathIndex >= 0){
			fprintf(file, "    \"carPathIndex\": %d,\n", obj->m_carPathIndex);
		}
		if(obj && obj->m_pedPathIndex >= 0){
			fprintf(file, "    \"pedPathIndex\": %d,\n", obj->m_pedPathIndex);
		}
		fprintf(file, "    \"isBigBuilding\": %s\n", inst->m_isBigBuilding ? "true" : "false");
		fprintf(file, "  }");
	}
	fprintf(file, "\n]\n");
}

static void
ExportObjectsCSV(FILE *file)
{
	fprintf(file, "instanceId,objectId,objectName,posX,posY,posZ,rotX,rotY,rotZ,rotW,area,numEffects,carPathIndex,pedPathIndex,isBigBuilding,drawDist1,drawDist2,drawDist3,minDrawDist,drawLast,additive,noFade,noZwrite,noShadows,ignoreDrawDist,isCodeGlass,isArtistGlass,noBackfaceCulling,isGarageDoor,isDamageable,isTree,isPalmTree,isDoor,isTimed,timeOn,timeOff\n");
	for(CPtrNode *node = instances.first; node != nil; node = node->next){
		ObjectInst *inst = (ObjectInst*)node->item;
		if(inst->m_isDeleted)
			continue;

		ObjectDef *obj = GetObjectDef(inst->m_objectId);
		const char *name = obj ? obj->m_name : "";

		fprintf(file, "%d,%d,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d,%s,",
			inst->m_id, inst->m_objectId, name,
			inst->m_translation.x, inst->m_translation.y, inst->m_translation.z,
			inst->m_rotation.x, inst->m_rotation.y,
			inst->m_rotation.z, inst->m_rotation.w,
			inst->m_area,
			obj ? obj->m_numEffects : 0,
			obj ? obj->m_carPathIndex : -1,
			obj ? obj->m_pedPathIndex : -1,
			inst->m_isBigBuilding ? "true" : "false");
		if(obj){
			fprintf(file, "%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
				obj->m_drawDist[0], obj->m_drawDist[1], obj->m_drawDist[2],
				obj->m_minDrawDist,
				obj->m_drawLast, obj->m_additive, obj->m_noFade, obj->m_noZwrite,
				obj->m_noShadows, obj->m_ignoreDrawDist,
				obj->m_isCodeGlass, obj->m_isArtistGlass, obj->m_noBackfaceCulling,
				obj->m_isGarageDoor, obj->m_isDamageable,
				obj->m_isTree, obj->m_isPalmTree, obj->m_isDoor,
				obj->m_isTimed, obj->m_timeOn, obj->m_timeOff);
		}else{
			fprintf(file, "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n");
		}
	}
}

static void
ExportPedPathsJSON(FILE *file)
{
	fprintf(file, "[\n");
	int numNodes = Path::GetNumPedNodes();
	bool first = true;
	for(int idx = 0; idx < numNodes; idx++){
		int base = idx / 12;
		int i = idx % 12;
		PathNode *node = Path::GetPedNode(base, i);
		if(node == nil)
			continue;

		if(!first) fprintf(file, ",\n");
		first = false;

		fprintf(file, "  {\n");
		fprintf(file, "    \"base\": %d, \"index\": %d,\n", base, i);
		fprintf(file, "    \"position\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
			node->x, node->y, node->z);
		fprintf(file, "    \"width\": %.4f,\n", node->width);
		fprintf(file, "    \"lanesIn\": %d, \"lanesOut\": %d,\n", node->lanesIn, node->lanesOut);
		fprintf(file, "    \"speed\": %d,\n", node->speed);
		fprintf(file, "    \"flags\": %d,\n", node->flags);
		fprintf(file, "    \"link\": %d, \"linkType\": %d,\n", node->link, node->linkType);
		fprintf(file, "    \"numLinks\": %d\n", node->numLinks);
		fprintf(file, "  }");
	}
	fprintf(file, "\n]\n");
}

static void
ExportPedPathsCSV(FILE *file)
{
	fprintf(file, "base,index,posX,posY,posZ,width,lanesIn,lanesOut,speed,flags,link,linkType,numLinks\n");
	int numNodes = Path::GetNumPedNodes();
	for(int idx = 0; idx < numNodes; idx++){
		int base = idx / 12;
		int i = idx % 12;
		PathNode *node = Path::GetPedNode(base, i);
		if(node == nil)
			continue;

		fprintf(file, "%d,%d,%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d,%d,%d,%d\n",
			base, i, node->x, node->y, node->z,
			node->width, node->lanesIn, node->lanesOut,
			node->speed, node->flags, node->link, node->linkType, node->numLinks);
	}
}

static void
ExportCarPathsJSON(FILE *file)
{
	fprintf(file, "[\n");
	int numNodes = Path::GetNumCarNodes();
	bool first = true;
	for(int idx = 0; idx < numNodes; idx++){
		int base = idx / 12;
		int i = idx % 12;
		PathNode *node = Path::GetCarNode(base, i);
		if(node == nil)
			continue;

		if(!first) fprintf(file, ",\n");
		first = false;

		fprintf(file, "  {\n");
		fprintf(file, "    \"base\": %d, \"index\": %d,\n", base, i);
		fprintf(file, "    \"position\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
			node->x, node->y, node->z);
		fprintf(file, "    \"width\": %.4f,\n", node->width);
		fprintf(file, "    \"lanesIn\": %d, \"lanesOut\": %d,\n", node->lanesIn, node->lanesOut);
		fprintf(file, "    \"speed\": %d,\n", node->speed);
		fprintf(file, "    \"flags\": %d,\n", node->flags);
		fprintf(file, "    \"density\": %.4f,\n", node->density);
		fprintf(file, "    \"link\": %d, \"linkType\": %d,\n", node->link, node->linkType);
		fprintf(file, "    \"numLinks\": %d\n", node->numLinks);
		fprintf(file, "  }");
	}
	fprintf(file, "\n]\n");
}

static void
ExportCarPathsCSV(FILE *file)
{
	fprintf(file, "base,index,posX,posY,posZ,width,lanesIn,lanesOut,speed,flags,density,link,linkType,numLinks\n");
	int numNodes = Path::GetNumCarNodes();
	for(int idx = 0; idx < numNodes; idx++){
		int base = idx / 12;
		int i = idx % 12;
		PathNode *node = Path::GetCarNode(base, i);
		if(node == nil)
			continue;

		fprintf(file, "%d,%d,%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d,%.4f,%d,%d,%d\n",
			base, i, node->x, node->y, node->z,
			node->width, node->lanesIn, node->lanesOut,
			node->speed, node->flags, node->density,
			node->link, node->linkType, node->numLinks);
	}
}

static void
ExportMapZonesJSON(FILE *file)
{
	fprintf(file, "[\n");
	int n = Zones::GetNumMapZones();
	bool first = true;
	for(int i = 0; i < n; i++){
		Zones::ZoneLabelInfo info;
		if(!Zones::GetMapZone(i, &info))
			continue;

		if(!first) fprintf(file, ",\n");
		first = false;

		fprintf(file, "  {\n");
		fprintf(file, "    \"index\": %d,\n", i);
		fprintf(file, "    \"name\": \"%s\",\n", info.name);
		fprintf(file, "    \"type\": %d,\n", info.type);
		fprintf(file, "    \"level\": %d,\n", info.level);
		fprintf(file, "    \"center\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
			info.center.x, info.center.y, info.center.z);
		fprintf(file, "    \"size\": { \"width\": %.4f, \"height\": %.4f }\n", info.width, info.height);
		fprintf(file, "  }");
	}
	fprintf(file, "\n]\n");
}

static void
ExportMapZonesCSV(FILE *file)
{
	fprintf(file, "index,name,type,level,centerX,centerY,centerZ,width,height\n");
	int n = Zones::GetNumMapZones();
	for(int i = 0; i < n; i++){
		Zones::ZoneLabelInfo info;
		if(!Zones::GetMapZone(i, &info))
			continue;

		fprintf(file, "%d,%s,%d,%d,%.4f,%.4f,%.4f,%.4f,%.4f\n",
			i, info.name, info.type, info.level,
			info.center.x, info.center.y, info.center.z,
			info.width, info.height);
	}
}

static void
ExportNavigZonesJSON(FILE *file)
{
	fprintf(file, "[\n");
	int n = Zones::GetNumNavigZones();
	bool first = true;
	for(int i = 0; i < n; i++){
		Zones::ZoneLabelInfo info;
		if(!Zones::GetNavigZone(i, &info))
			continue;

		if(!first) fprintf(file, ",\n");
		first = false;

		fprintf(file, "  {\n");
		fprintf(file, "    \"index\": %d,\n", i);
		fprintf(file, "    \"name\": \"%s\",\n", info.name);
		fprintf(file, "    \"type\": %d,\n", info.type);
		fprintf(file, "    \"typeName\": \"%s\",\n", getZoneTypeName(info.type));
		fprintf(file, "    \"level\": %d,\n", info.level);
		fprintf(file, "    \"center\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
			info.center.x, info.center.y, info.center.z);
		fprintf(file, "    \"size\": { \"width\": %.4f, \"height\": %.4f }\n", info.width, info.height);
		fprintf(file, "  }");
	}
	fprintf(file, "\n]\n");
}

static void
ExportNavigZonesCSV(FILE *file)
{
	fprintf(file, "index,name,type,typeName,level,centerX,centerY,centerZ,width,height\n");
	int n = Zones::GetNumNavigZones();
	for(int i = 0; i < n; i++){
		Zones::ZoneLabelInfo info;
		if(!Zones::GetNavigZone(i, &info))
			continue;

		fprintf(file, "%d,%s,%d,%s,%d,%.4f,%.4f,%.4f,%.4f,%.4f\n",
			i, info.name, info.type, getZoneTypeName(info.type), info.level,
			info.center.x, info.center.y, info.center.z,
			info.width, info.height);
	}
}

static void
ExportAttribZonesJSON(FILE *file)
{
	fprintf(file, "[\n");
	int n = Zones::GetNumAttribZones();
	bool first = true;
	for(int i = 0; i < n; i++){
		Zones::AttribZoneLabelInfo info;
		if(!Zones::GetAttribZone(i, &info))
			continue;

		if(!first) fprintf(file, ",\n");
		first = false;

		fprintf(file, "  {\n");
		fprintf(file, "    \"index\": %d,\n", i);
		fprintf(file, "    \"center\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
			info.center.x, info.center.y, info.center.z);
		fprintf(file, "    \"size\": { \"width\": %.4f, \"height\": %.4f },\n", info.width, info.height);
		fprintf(file, "    \"attribs\": %d,\n", info.attribs);
		fprintf(file, "    \"wantedLevelDrop\": %d\n", info.wantedLevelDrop);
		fprintf(file, "  }");
	}
	fprintf(file, "\n]\n");
}

static void
ExportAttribZonesCSV(FILE *file)
{
	fprintf(file, "index,centerX,centerY,centerZ,width,height,attribs,wantedLevelDrop\n");
	int n = Zones::GetNumAttribZones();
	for(int i = 0; i < n; i++){
		Zones::AttribZoneLabelInfo info;
		if(!Zones::GetAttribZone(i, &info))
			continue;

		fprintf(file, "%d,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d\n",
			i, info.center.x, info.center.y, info.center.z,
			info.width, info.height, info.attribs, info.wantedLevelDrop);
	}
}

static void
Export2dfxJSON(FILE *file)
{
	fprintf(file, "[\n");
	bool first = true;
	for(CPtrNode *node = instances.first; node != nil; node = node->next){
		ObjectInst *inst = (ObjectInst*)node->item;
		if(inst->m_isDeleted)
			continue;

		ObjectDef *obj = GetObjectDef(inst->m_objectId);
		if(obj == nil || obj->m_numEffects == 0)
			continue;

		for(int i = 0; i < obj->m_numEffects; i++){
			Effect *e = Effects::GetEffect(obj->m_effectIndex + i);
			if(e == nil)
				continue;

			rw::V3d worldPos;
			rw::V3d::transformPoints(&worldPos, &e->pos, 1, &inst->m_matrix);

			if(!first) fprintf(file, ",\n");
			first = false;

			fprintf(file, "  {\n");
			fprintf(file, "    \"objectId\": %d,\n", inst->m_objectId);
			fprintf(file, "    \"objectName\": \"%s\",\n", obj->m_name);
			fprintf(file, "    \"effectIndex\": %d,\n", i);
			fprintf(file, "    \"type\": %d,\n", e->type);
			fprintf(file, "    \"typeName\": \"%s\",\n", getEffectTypeName(e->type));
			fprintf(file, "    \"localPos\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
				e->pos.x, e->pos.y, e->pos.z);
			fprintf(file, "    \"worldPos\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
				worldPos.x, worldPos.y, worldPos.z);

			switch(e->type){
			case FX_LIGHT:
				fprintf(file, "    \"coronaTex\": \"%.12s\",\n", e->light.coronaTex);
				fprintf(file, "    \"coronaSize\": %.1f,\n", e->light.coronaSize);
				fprintf(file, "    \"shadowSize\": %.1f,\n", e->light.shadowSize);
				fprintf(file, "    \"flashiness\": %d,\n", e->light.flashiness);
				fprintf(file, "    \"lodDist\": %.1f,\n", e->light.lodDist);
				fprintf(file, "    \"flags\": 0x%X\n", e->light.flags);
				break;
			case FX_PARTICLE:
				fprintf(file, "    \"particleName\": \"%.20s\",\n", e->prtcl.name);
				fprintf(file, "    \"particleType\": %d,\n", e->prtcl.particleType);
				fprintf(file, "    \"size\": %.1f\n", e->prtcl.size);
				break;
			case FX_ROADSIGN:
				fprintf(file, "    \"width\": %.1f,\n", e->roadsign.width);
				fprintf(file, "    \"height\": %.1f,\n", e->roadsign.height);
				fprintf(file, "    \"text\": \"%.12s\"\n", e->roadsign.text[0]);
				break;
			default:
				fprintf(file, "    \"extra\": 0\n");
				break;
			}
			fprintf(file, "  }");
		}
	}
	fprintf(file, "\n]\n");
}

static void
Export2dfxCSV(FILE *file)
{
	fprintf(file, "objectId,objectName,effectIndex,type,typeName,localX,localY,localZ,worldX,worldY,worldZ,extra1,extra2,extra3\n");
	for(CPtrNode *node = instances.first; node != nil; node = node->next){
		ObjectInst *inst = (ObjectInst*)node->item;
		if(inst->m_isDeleted)
			continue;

		ObjectDef *obj = GetObjectDef(inst->m_objectId);
		if(obj == nil || obj->m_numEffects == 0)
			continue;

		for(int i = 0; i < obj->m_numEffects; i++){
			Effect *e = Effects::GetEffect(obj->m_effectIndex + i);
			if(e == nil)
				continue;

			rw::V3d worldPos;
			rw::V3d::transformPoints(&worldPos, &e->pos, 1, &inst->m_matrix);

			fprintf(file, "%d,%s,%d,%d,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,",
				inst->m_objectId, obj->m_name, i, e->type, getEffectTypeName(e->type),
				e->pos.x, e->pos.y, e->pos.z,
				worldPos.x, worldPos.y, worldPos.z);

			switch(e->type){
			case FX_LIGHT:
				fprintf(file, "%.1f,%.1f,%d\n", e->light.coronaSize, e->light.shadowSize, e->light.flashiness);
				break;
			case FX_PARTICLE:
				fprintf(file, "%d,%.1f,%d\n", e->prtcl.particleType, e->prtcl.size, 0);
				break;
			case FX_ROADSIGN:
				fprintf(file, "%.1f,%.1f,0\n", e->roadsign.width, e->roadsign.height);
				break;
			default:
				fprintf(file, "0,0,0\n");
				break;
			}
		}
	}
}

void
ExportData(const char *filename, ExportDataType type, int format)
{
	FILE *file = fopen(filename, "w");
	if(file == nil){
		return;
	}

	switch(type){
	case EXPORT_OBJECTS:
		if(format == EXPORT_JSON) ExportObjectsJSON(file);
		else ExportObjectsCSV(file);
		break;
	case EXPORT_PED_PATHS:
		if(format == EXPORT_JSON) ExportPedPathsJSON(file);
		else ExportPedPathsCSV(file);
		break;
	case EXPORT_CAR_PATHS:
		if(format == EXPORT_JSON) ExportCarPathsJSON(file);
		else ExportCarPathsCSV(file);
		break;
	case EXPORT_MAP_ZONES:
		if(format == EXPORT_JSON) ExportMapZonesJSON(file);
		else ExportMapZonesCSV(file);
		break;
	case EXPORT_NAVIG_ZONES:
		if(format == EXPORT_JSON) ExportNavigZonesJSON(file);
		else ExportNavigZonesCSV(file);
		break;
	case EXPORT_ATTRIB_ZONES:
		if(format == EXPORT_JSON) ExportAttribZonesJSON(file);
		else ExportAttribZonesCSV(file);
		break;
	case EXPORT_2DFX:
		if(format == EXPORT_JSON) Export2dfxJSON(file);
		else Export2dfxCSV(file);
		break;
	case EXPORT_CARREC:
		if(format == EXPORT_JSON) ExportCarrecJSON(file);
		else ExportCarrecCSV(file);
		break;
	}

	fclose(file);
}

static void
ExportCarrecJSON(FILE *file)
{
	fprintf(file, "[\n");
	int numRecs = Carrec::GetNumRecordings();
	bool first = true;
	for(int r = 0; r < numRecs; r++){
		CarrecRecording *rec = Carrec::GetRecording(r);
		if(rec == nil)
			continue;

		if(!first) fprintf(file, ",\n");
		first = false;

		fprintf(file, "  {\n");
		fprintf(file, "    \"id\": %d,\n", rec->id);
		fprintf(file, "    \"numPoints\": %d,\n", (int)rec->points.size());
		fprintf(file, "    \"points\": [\n");

		for(size_t p = 0; p < rec->points.size(); p++){
			CarrecPoint &pt = rec->points[p];
			fprintf(file, "      {\"time\": %d, ", pt.time);
			fprintf(file, "\"vel\": { \"x\": %.6f, \"y\": %.6f, \"z\": %.6f }, ",
				pt.velX / 16383.5, pt.velY / 16383.5, pt.velZ / 16383.5);
			fprintf(file, "\"right\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f }, ",
				pt.rightX / 127.0f, pt.rightY / 127.0f, pt.rightZ / 127.0f);
			fprintf(file, "\"top\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f }, ",
				pt.topX / 127.0f, pt.topY / 127.0f, pt.topZ / 127.0f);
			fprintf(file, "\"steering\": %.2f, \"gas\": %.2f, \"brake\": %.2f, \"handbrake\": %d, ",
				pt.steering / 20.0f, pt.gas / 100.0f, pt.brake / 100.0f, pt.handbrake);
			fprintf(file, "\"pos\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f }}%s\n",
				pt.posX, pt.posY, pt.posZ,
				p < rec->points.size() - 1 ? "," : "");
		}

		fprintf(file, "    ]\n");
		fprintf(file, "  }");
	}
	fprintf(file, "\n]\n");
}

static void
ExportCarrecCSV(FILE *file)
{
	fprintf(file, "recId,pointIdx,time,velX,velY,velZ,rightX,rightY,rightZ,topX,topY,topZ,steering,gas,brake,handbrake,posX,posY,posZ\n");
	int numRecs = Carrec::GetNumRecordings();
	for(int r = 0; r < numRecs; r++){
		CarrecRecording *rec = Carrec::GetRecording(r);
		if(rec == nil)
			continue;

		for(size_t p = 0; p < rec->points.size(); p++){
			CarrecPoint &pt = rec->points[p];
			fprintf(file, "%d,%d,%d,%.6f,%.6f,%.6f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f,%d,%.4f,%.4f,%.4f\n",
				rec->id, (int)p, pt.time,
				pt.velX / 16383.5, pt.velY / 16383.5, pt.velZ / 16383.5,
				pt.rightX / 127.0f, pt.rightY / 127.0f, pt.rightZ / 127.0f,
				pt.topX / 127.0f, pt.topY / 127.0f, pt.topZ / 127.0f,
				pt.steering / 20.0f, pt.gas / 100.0f, pt.brake / 100.0f, pt.handbrake,
				pt.posX, pt.posY, pt.posZ);
		}
	}
}
