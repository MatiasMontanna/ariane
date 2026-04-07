#include "euryopa.h"
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
ExportObjectsJSON(FILE *f)
{
	fprintf(f, "[\n");
	bool first = true;
	for(CPtrNode *p = instances.first; p; p; p = p->next){
		ObjectInst *inst = (ObjectInst*)p->item;
		if(inst->m_isDeleted)
			continue;

		ObjectDef *obj = GetObjectDef(inst->m_objectId);

		if(!first) fprintf(f, ",\n");
		first = false;

		fprintf(f, "  {\n");
		fprintf(f, "    \"instanceId\": %d,\n", inst->m_id);
		fprintf(f, "    \"objectId\": %d,\n", inst->m_objectId);
		if(obj){
			fprintf(f, "    \"objectName\": \"%s\",\n", obj->m_name);
		}
		fprintf(f, "    \"position\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
			inst->m_translation.x, inst->m_translation.y, inst->m_translation.z);
		fprintf(f, "    \"rotation\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f, \"w\": %.4f },\n",
			inst->m_rotation.x, inst->m_rotation.y,
			inst->m_rotation.z, inst->m_rotation.w);
		fprintf(f, "    \"area\": %d,\n", inst->m_area);
		if(obj && obj->m_numEffects > 0){
			fprintf(f, "    \"numEffects\": %d,\n", obj->m_numEffects);
		}
		if(obj && obj->m_carPathIndex >= 0){
			fprintf(f, "    \"carPathIndex\": %d,\n", obj->m_carPathIndex);
		}
		if(obj && obj->m_pedPathIndex >= 0){
			fprintf(f, "    \"pedPathIndex\": %d,\n", obj->m_pedPathIndex);
		}
		fprintf(f, "    \"isBigBuilding\": %s\n", inst->m_isBigBuilding ? "true" : "false");
		fprintf(f, "  }");
	}
	fprintf(f, "\n]\n");
}

static void
ExportObjectsCSV(FILE *f)
{
	fprintf(f, "instanceId,objectId,objectName,posX,posY,posZ,rotX,rotY,rotZ,rotW,area,numEffects,carPathIndex,pedPathIndex,isBigBuilding\n");
	for(CPtrNode *p = instances.first; p; p; p = p->next){
		ObjectInst *inst = (ObjectInst*)p->item;
		if(inst->m_isDeleted)
			continue;

		ObjectDef *obj = GetObjectDef(inst->m_objectId);
		const char *name = obj ? obj->m_name : "";

		fprintf(f, "%d,%d,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d,%s\n",
			inst->m_id, inst->m_objectId, name,
			inst->m_translation.x, inst->m_translation.y, inst->m_translation.z,
			inst->m_rotation.x, inst->m_rotation.y,
			inst->m_rotation.z, inst->m_rotation.w,
			inst->m_area,
			obj ? obj->m_numEffects : 0,
			obj ? obj->m_carPathIndex : -1,
			obj ? obj->m_pedPathIndex : -1,
			inst->m_isBigBuilding ? "true" : "false");
	}
}

static void
ExportPedPathsJSON(FILE *f)
{
	fprintf(f, "[\n");
	int numNodes = Path::GetNumPedNodes();
	bool first = true;
	for(int idx = 0; idx < numNodes; idx++){
		int base = idx / 12;
		int i = idx % 12;
		PathNode *node = Path::GetPedNode(base, i);
		if(node == nil)
			continue;

		if(!first) fprintf(f, ",\n");
		first = false;

		fprintf(f, "  {\n");
		fprintf(f, "    \"base\": %d, \"index\": %d,\n", base, i);
		fprintf(f, "    \"position\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
			node->x, node->y, node->z);
		fprintf(f, "    \"width\": %.4f,\n", node->width);
		fprintf(f, "    \"lanesIn\": %d, \"lanesOut\": %d,\n", node->lanesIn, node->lanesOut);
		fprintf(f, "    \"speed\": %d,\n", node->speed);
		fprintf(f, "    \"flags\": %d,\n", node->flags);
		fprintf(f, "    \"link\": %d, \"linkType\": %d,\n", node->link, node->linkType);
		fprintf(f, "    \"numLinks\": %d\n", node->numLinks);
		fprintf(f, "  }");
	}
	fprintf(f, "\n]\n");
}

static void
ExportPedPathsCSV(FILE *f)
{
	fprintf(f, "base,index,posX,posY,posZ,width,lanesIn,lanesOut,speed,flags,link,linkType,numLinks\n");
	int numNodes = Path::GetNumPedNodes();
	for(int idx = 0; idx < numNodes; idx++){
		int base = idx / 12;
		int i = idx % 12;
		PathNode *node = Path::GetPedNode(base, i);
		if(node == nil)
			continue;

		fprintf(f, "%d,%d,%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d,%d,%d,%d\n",
			base, i, node->x, node->y, node->z,
			node->width, node->lanesIn, node->lanesOut,
			node->speed, node->flags, node->link, node->linkType, node->numLinks);
	}
}

static void
ExportCarPathsJSON(FILE *f)
{
	fprintf(f, "[\n");
	int numNodes = Path::GetNumCarNodes();
	bool first = true;
	for(int idx = 0; idx < numNodes; idx++){
		int base = idx / 12;
		int i = idx % 12;
		PathNode *node = Path::GetCarNode(base, i);
		if(node == nil)
			continue;

		if(!first) fprintf(f, ",\n");
		first = false;

		fprintf(f, "  {\n");
		fprintf(f, "    \"base\": %d, \"index\": %d,\n", base, i);
		fprintf(f, "    \"position\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
			node->x, node->y, node->z);
		fprintf(f, "    \"width\": %.4f,\n", node->width);
		fprintf(f, "    \"lanesIn\": %d, \"lanesOut\": %d,\n", node->lanesIn, node->lanesOut);
		fprintf(f, "    \"speed\": %d,\n", node->speed);
		fprintf(f, "    \"flags\": %d,\n", node->flags);
		fprintf(f, "    \"density\": %.4f,\n", node->density);
		fprintf(f, "    \"link\": %d, \"linkType\": %d,\n", node->link, node->linkType);
		fprintf(f, "    \"numLinks\": %d\n", node->numLinks);
		fprintf(f, "  }");
	}
	fprintf(f, "\n]\n");
}

static void
ExportCarPathsCSV(FILE *f)
{
	fprintf(f, "base,index,posX,posY,posZ,width,lanesIn,lanesOut,speed,flags,density,link,linkType,numLinks\n");
	int numNodes = Path::GetNumCarNodes();
	for(int idx = 0; idx < numNodes; idx++){
		int base = idx / 12;
		int i = idx % 12;
		PathNode *node = Path::GetCarNode(base, i);
		if(node == nil)
			continue;

		fprintf(f, "%d,%d,%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d,%.4f,%d,%d,%d\n",
			base, i, node->x, node->y, node->z,
			node->width, node->lanesIn, node->lanesOut,
			node->speed, node->flags, node->density,
			node->link, node->linkType, node->numLinks);
	}
}

static void
ExportMapZonesJSON(FILE *f)
{
	fprintf(f, "[\n");
	int n = Zones::GetNumMapZones();
	bool first = true;
	for(int i = 0; i < n; i++){
		Zones::ZoneLabelInfo info;
		if(!Zones::GetMapZone(i, &info))
			continue;

		if(!first) fprintf(f, ",\n");
		first = false;

		fprintf(f, "  {\n");
		fprintf(f, "    \"index\": %d,\n", i);
		fprintf(f, "    \"name\": \"%s\",\n", info.name);
		fprintf(f, "    \"type\": %d,\n", info.type);
		fprintf(f, "    \"level\": %d,\n", info.level);
		fprintf(f, "    \"center\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
			info.center.x, info.center.y, info.center.z);
		fprintf(f, "    \"size\": { \"width\": %.4f, \"height\": %.4f }\n", info.width, info.height);
		fprintf(f, "  }");
	}
	fprintf(f, "\n]\n");
}

static void
ExportMapZonesCSV(FILE *f)
{
	fprintf(f, "index,name,type,level,centerX,centerY,centerZ,width,height\n");
	int n = Zones::GetNumMapZones();
	for(int i = 0; i < n; i++){
		Zones::ZoneLabelInfo info;
		if(!Zones::GetMapZone(i, &info))
			continue;

		fprintf(f, "%d,%s,%d,%d,%.4f,%.4f,%.4f,%.4f,%.4f\n",
			i, info.name, info.type, info.level,
			info.center.x, info.center.y, info.center.z,
			info.width, info.height);
	}
}

static void
ExportNavigZonesJSON(FILE *f)
{
	fprintf(f, "[\n");
	int n = Zones::GetNumNavigZones();
	bool first = true;
	for(int i = 0; i < n; i++){
		Zones::ZoneLabelInfo info;
		if(!Zones::GetNavigZone(i, &info))
			continue;

		if(!first) fprintf(f, ",\n");
		first = false;

		fprintf(f, "  {\n");
		fprintf(f, "    \"index\": %d,\n", i);
		fprintf(f, "    \"name\": \"%s\",\n", info.name);
		fprintf(f, "    \"type\": %d,\n", info.type);
		fprintf(f, "    \"typeName\": \"%s\",\n", getZoneTypeName(info.type));
		fprintf(f, "    \"level\": %d,\n", info.level);
		fprintf(f, "    \"center\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
			info.center.x, info.center.y, info.center.z);
		fprintf(f, "    \"size\": { \"width\": %.4f, \"height\": %.4f }\n", info.width, info.height);
		fprintf(f, "  }");
	}
	fprintf(f, "\n]\n");
}

static void
ExportNavigZonesCSV(FILE *f)
{
	fprintf(f, "index,name,type,typeName,level,centerX,centerY,centerZ,width,height\n");
	int n = Zones::GetNumNavigZones();
	for(int i = 0; i < n; i++){
		Zones::ZoneLabelInfo info;
		if(!Zones::GetNavigZone(i, &info))
			continue;

		fprintf(f, "%d,%s,%d,%s,%d,%.4f,%.4f,%.4f,%.4f,%.4f\n",
			i, info.name, info.type, getZoneTypeName(info.type), info.level,
			info.center.x, info.center.y, info.center.z,
			info.width, info.height);
	}
}

static void
ExportAttribZonesJSON(FILE *f)
{
	fprintf(f, "[\n");
	int n = Zones::GetNumAttribZones();
	bool first = true;
	for(int i = 0; i < n; i++){
		Zones::AttribZoneLabelInfo info;
		if(!Zones::GetAttribZone(i, &info))
			continue;

		if(!first) fprintf(f, ",\n");
		first = false;

		fprintf(f, "  {\n");
		fprintf(f, "    \"index\": %d,\n", i);
		fprintf(f, "    \"center\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
			info.center.x, info.center.y, info.center.z);
		fprintf(f, "    \"size\": { \"width\": %.4f, \"height\": %.4f },\n", info.width, info.height);
		fprintf(f, "    \"attribs\": %d,\n", info.attribs);
		fprintf(f, "    \"wantedLevelDrop\": %d\n", info.wantedLevelDrop);
		fprintf(f, "  }");
	}
	fprintf(f, "\n]\n");
}

static void
ExportAttribZonesCSV(FILE *f)
{
	fprintf(f, "index,centerX,centerY,centerZ,width,height,attribs,wantedLevelDrop\n");
	int n = Zones::GetNumAttribZones();
	for(int i = 0; i < n; i++){
		Zones::AttribZoneLabelInfo info;
		if(!Zones::GetAttribZone(i, &info))
			continue;

		fprintf(f, "%d,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d\n",
			i, info.center.x, info.center.y, info.center.z,
			info.width, info.height, info.attribs, info.wantedLevelDrop);
	}
}

static void
Export2dfxJSON(FILE *f)
{
	fprintf(f, "[\n");
	bool first = true;
	for(CPtrNode *p = instances.first; p; p; p = p->next){
		ObjectInst *inst = (ObjectInst*)p->item;
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

			if(!first) fprintf(f, ",\n");
			first = false;

			fprintf(f, "  {\n");
			fprintf(f, "    \"objectId\": %d,\n", inst->m_objectId);
			fprintf(f, "    \"effectIndex\": %d,\n", i);
			fprintf(f, "    \"type\": %d,\n", e->type);
			fprintf(f, "    \"typeName\": \"%s\",\n", getEffectTypeName(e->type));
			fprintf(f, "    \"localPos\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
				e->pos.x, e->pos.y, e->pos.z);
			fprintf(f, "    \"worldPos\": { \"x\": %.4f, \"y\": %.4f, \"z\": %.4f },\n",
				worldPos.x, worldPos.y, worldPos.z);

			switch(e->type){
			case FX_LIGHT:
				fprintf(f, "    \"coronaTex\": \"%.12s\",\n", e->light.coronaTex);
				fprintf(f, "    \"coronaSize\": %.1f,\n", e->light.coronaSize);
				fprintf(f, "    \"shadowSize\": %.1f,\n", e->light.shadowSize);
				fprintf(f, "    \"flashiness\": %d,\n", e->light.flashiness);
				fprintf(f, "    \"lodDist\": %.1f,\n", e->light.lodDist);
				fprintf(f, "    \"flags\": 0x%X\n", e->light.flags);
				break;
			case FX_PARTICLE:
				fprintf(f, "    \"particleName\": \"%.20s\",\n", e->prtcl.name);
				fprintf(f, "    \"particleType\": %d,\n", e->prtcl.particleType);
				fprintf(f, "    \"size\": %.1f\n", e->prtcl.size);
				break;
			case FX_ROADSIGN:
				fprintf(f, "    \"width\": %.1f,\n", e->roadsign.width);
				fprintf(f, "    \"height\": %.1f,\n", e->roadsign.height);
				fprintf(f, "    \"text\": \"%.12s\"\n", e->roadsign.text[0]);
				break;
			default:
				fprintf(f, "    \"extra\": 0\n");
				break;
			}
			fprintf(f, "  }");
		}
	}
	fprintf(f, "\n]\n");
}

static void
Export2dfxCSV(FILE *f)
{
	fprintf(f, "objectId,effectIndex,type,typeName,localX,localY,localZ,worldX,worldY,worldZ,extra1,extra2,extra3\n");
	for(CPtrNode *p = instances.first; p; p; p = p->next){
		ObjectInst *inst = (ObjectInst*)p->item;
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

			fprintf(f, "%d,%d,%d,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,",
				inst->m_objectId, i, e->type, getEffectTypeName(e->type),
				e->pos.x, e->pos.y, e->pos.z,
				worldPos.x, worldPos.y, worldPos.z);

			switch(e->type){
			case FX_LIGHT:
				fprintf(f, "%.1f,%.1f,%d\n", e->light.coronaSize, e->light.shadowSize, e->light.flashiness);
				break;
			case FX_PARTICLE:
				fprintf(f, "%d,%.1f,%d\n", e->prtcl.particleType, e->prtcl.size, 0);
				break;
			case FX_ROADSIGN:
				fprintf(f, "%.1f,%.1f,0\n", e->roadsign.width, e->roadsign.height);
				break;
			default:
				fprintf(f, "0,0,0\n");
				break;
			}
		}
	}
}

void
ExportData(const char *filename, ExportDataType type, int format)
{
	FILE *f = fopen(filename, "w");
	if(f == nil){
		return;
	}

	switch(type){
	case EXPORT_OBJECTS:
		if(format == EXPORT_JSON) ExportObjectsJSON(f);
		else ExportObjectsCSV(f);
		break;
	case EXPORT_PED_PATHS:
		if(format == EXPORT_JSON) ExportPedPathsJSON(f);
		else ExportPedPathsCSV(f);
		break;
	case EXPORT_CAR_PATHS:
		if(format == EXPORT_JSON) ExportCarPathsJSON(f);
		else ExportCarPathsCSV(f);
		break;
	case EXPORT_MAP_ZONES:
		if(format == EXPORT_JSON) ExportMapZonesJSON(f);
		else ExportMapZonesCSV(f);
		break;
	case EXPORT_NAVIG_ZONES:
		if(format == EXPORT_JSON) ExportNavigZonesJSON(f);
		else ExportNavigZonesCSV(f);
		break;
	case EXPORT_ATTRIB_ZONES:
		if(format == EXPORT_JSON) ExportAttribZonesJSON(f);
		else ExportAttribZonesCSV(f);
		break;
	case EXPORT_2DFX:
		if(format == EXPORT_JSON) Export2dfxJSON(f);
		else Export2dfxCSV(f);
		break;
	}

	fclose(f);
}
