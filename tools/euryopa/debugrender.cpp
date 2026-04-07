#include "euryopa.h"

struct DebugLine
{
	rw::V3d v1;
	rw::V3d v2;
	rw::RGBA col1;
	rw::RGBA col2;
};

#define MAXDEBUGLINES 262144
#define DEBUGLINE_PADDING 64
static DebugLine *debugLines;
static int numDebugLines;

static void
EnsureDebugLineBuffer(void)
{
	if(debugLines == nil)
		debugLines = (DebugLine*)calloc(MAXDEBUGLINES + DEBUGLINE_PADDING, sizeof(DebugLine));
}

#define TEMPBUFFERVERTSIZE 256
#define TEMPBUFFERINDEXSIZE 1024
static int TempBufferIndicesStored;
static int TempBufferVerticesStored;
static rw::RWDEVICE::Im3DVertex TempVertexBuffer[TEMPBUFFERVERTSIZE];
static uint16 TempIndexBuffer[TEMPBUFFERINDEXSIZE];

static void
RenderAndEmptyRenderBuffer(void)
{
	assert(TempBufferVerticesStored <= TEMPBUFFERVERTSIZE);
	assert(TempBufferIndicesStored <= TEMPBUFFERINDEXSIZE);
	if(TempBufferVerticesStored){
		rw::im3d::Transform(TempVertexBuffer, TempBufferVerticesStored, nil, rw::im3d::EVERYTHING);
		rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPELINELIST, TempIndexBuffer, TempBufferIndicesStored);
		rw::im3d::End();
	}
	TempBufferVerticesStored = 0;
	TempBufferIndicesStored = 0;
}

static void
RenderLine(float x1, float y1, float z1, float x2, float y2, float z2, rw::RGBA c1, rw::RGBA c2)
{
	int i;
	if(TempBufferVerticesStored+2 >= TEMPBUFFERVERTSIZE ||
	   TempBufferIndicesStored+2 >= TEMPBUFFERINDEXSIZE)
		RenderAndEmptyRenderBuffer();

	i = TempBufferVerticesStored;
	TempVertexBuffer[i].setX(x1);
	TempVertexBuffer[i].setY(y1);
	TempVertexBuffer[i].setZ(z1);
	TempVertexBuffer[i].setColor(c1.red, c1.green, c1.blue, c1.alpha);
	TempVertexBuffer[i+1].setX(x2);
	TempVertexBuffer[i+1].setY(y2);
	TempVertexBuffer[i+1].setZ(z2);
	TempVertexBuffer[i+1].setColor(c2.red, c2.green, c2.blue, c2.alpha);
	TempBufferVerticesStored += 2;

	TempIndexBuffer[TempBufferIndicesStored++] = i;
	TempIndexBuffer[TempBufferIndicesStored++] = i+1;
}

void
RenderDebugLines(void)
{
	int i;

	rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);

	for(i = 0; i < numDebugLines; i++)
		RenderLine(debugLines[i].v1.x, debugLines[i].v1.y, debugLines[i].v1.z,
			debugLines[i].v2.x, debugLines[i].v2.y, debugLines[i].v2.z,
			debugLines[i].col1, debugLines[i].col2);
	RenderAndEmptyRenderBuffer();
	numDebugLines = 0;
}

static void
AddDebugLine(float x1, float y1, float z1, float x2, float y2, float z2, rw::RGBA c1, rw::RGBA c2)
{
	EnsureDebugLineBuffer();
	if(debugLines == nil || numDebugLines >= MAXDEBUGLINES)
		return;
	DebugLine *line = &debugLines[numDebugLines++];
	line->v1.x = x1;
	line->v1.y = y1;
	line->v1.z = z1;
	line->v2.x = x2;
	line->v2.y = y2;
	line->v2.z = z2;
	line->col1 = c1;
	line->col2 = c2;
}

void RenderLine(rw::V3d v1, rw::V3d v2, rw::RGBA c1, rw::RGBA c2) { AddDebugLine(v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, c1, c2); }

void
RenderWireBoxVerts(rw::V3d *verts, rw::RGBA col)
{
	RenderLine(verts[0], verts[1], col, col);
	RenderLine(verts[1], verts[3], col, col);
	RenderLine(verts[3], verts[2], col, col);
	RenderLine(verts[2], verts[0], col, col);

	RenderLine(verts[0+4], verts[1+4], col, col);
	RenderLine(verts[1+4], verts[3+4], col, col);
	RenderLine(verts[3+4], verts[2+4], col, col);
	RenderLine(verts[2+4], verts[0+4], col, col);

	RenderLine(verts[0], verts[4], col, col);
	RenderLine(verts[1], verts[5], col, col);
	RenderLine(verts[2], verts[6], col, col);
	RenderLine(verts[3], verts[7], col, col);
}

void
RenderWireBox(CBox *box, rw::RGBA col, rw::Matrix *xform)
{
	rw::V3d verts[8];
	verts[0].x = box->min.x;
	verts[0].y = box->min.y;
	verts[0].z = box->min.z;
	verts[1].x = box->max.x;
	verts[1].y = box->min.y;
	verts[1].z = box->min.z;
	verts[2].x = box->min.x;
	verts[2].y = box->max.y;
	verts[2].z = box->min.z;
	verts[3].x = box->max.x;
	verts[3].y = box->max.y;
	verts[3].z = box->min.z;
	verts[4].x = box->min.x;
	verts[4].y = box->min.y;
	verts[4].z = box->max.z;
	verts[5].x = box->max.x;
	verts[5].y = box->min.y;
	verts[5].z = box->max.z;
	verts[6].x = box->min.x;
	verts[6].y = box->max.y;
	verts[6].z = box->max.z;
	verts[7].x = box->max.x;
	verts[7].y = box->max.y;
	verts[7].z = box->max.z;
	if(xform)
		rw::V3d::transformPoints(verts, verts, 8, xform);

	RenderWireBoxVerts(verts, col);
}

void
RenderSphereAsWireBox(CSphere *sphere, rw::RGBA col, rw::Matrix *xform)
{
	CBox box;
	rw::V3d sz = { 1.0f, 1.0f, 1.0f };
	sz = rw::scale(sz,sphere->radius*0.5f);
	box.min = rw::sub(sphere->center, sz);
	box.max = rw::add(sphere->center, sz);
	RenderWireBox(&box, col, xform);
}

void
RenderSphereAsCross(CSphere *sphere, rw::RGBA col, rw::Matrix *xform)
{
	using namespace rw;
	float off = sphere->radius*0.5f;
	RenderLine(sub(sphere->center, makeV3d(off,0.0f,0.0f)),
	           add(sphere->center, makeV3d(off,0.0f,0.0f)),
	           col, col);
	RenderLine(sub(sphere->center, makeV3d(0.0f,off,0.0f)),
	           add(sphere->center, makeV3d(0.0f,off,0.0f)),
	           col, col);
	RenderLine(sub(sphere->center, makeV3d(0.0f,0.0f,off)),
	           add(sphere->center, makeV3d(0.0f,0.0f,off)),
	           col, col);
}

void
RenderWireSphere(CSphere *sphere, rw::RGBA col, rw::Matrix *xform)
{
	rw::V3d c;
	rw::V3d verts[6];
	c = sphere->center;
	if(xform)
		rw::V3d::transformPoints(&c, &c, 1, xform);
	verts[0] = verts[1] = verts[2] = verts[3] = verts[4] = verts[5] = c;
	verts[0].z += sphere->radius;	// top
	verts[1].z -= sphere->radius;	// bottom
	verts[2].x += sphere->radius;
	verts[3].x -= sphere->radius;
	verts[4].y += sphere->radius;
	verts[5].y -= sphere->radius;

	RenderLine(verts[0], verts[2], col, col);
	RenderLine(verts[0], verts[3], col, col);
	RenderLine(verts[0], verts[4], col, col);
	RenderLine(verts[0], verts[5], col, col);
	RenderLine(verts[1], verts[2], col, col);
	RenderLine(verts[1], verts[3], col, col);
	RenderLine(verts[1], verts[4], col, col);
}

void
RenderWireTriangle(rw::V3d *v1, rw::V3d *v2, rw::V3d *v3, rw::RGBA col, rw::Matrix *xform)
{
	rw::V3d verts[3];
	verts[0] = *v1;
	verts[1] = *v2;
	verts[2] = *v3;
	if(xform)
		rw::V3d::transformPoints(verts, verts, 3, xform);
	RenderLine(verts[0], verts[1], col, col);
	RenderLine(verts[1], verts[2], col, col);
	RenderLine(verts[2], verts[0], col, col);
}

void
RenderAxesWidget(rw::V3d pos, rw::V3d x, rw::V3d y, rw::V3d z)
{
	rw::RGBA red = { 255, 0, 0, 255 };
	rw::RGBA green = { 0, 255, 0, 255 };
	rw::RGBA blue = { 0, 0, 255, 255 };
	RenderLine(pos, add(pos, x), red, red);
	RenderLine(pos, add(pos, y), green, green);
	RenderLine(pos, add(pos, z), blue, blue);
}

static void
RenderLabelAtScreen(ImDrawList* drawList, float sx, float sy, const char* text, ImU32 color)
{
	ImFont* font = ImGui::GetFont();
	float fontSize = ImGui::GetFontSize();
	ImVec2 textSize = ImGui::CalcTextSize(text);
	float x = sx - textSize.x * 0.5f;
	drawList->AddText(font, fontSize, ImVec2(x + 1.0f, sy + 1.0f), IM_COL32_BLACK, text);
	drawList->AddText(font, fontSize, ImVec2(x, sy), color, text);
}

static void
RenderLabelLine(ImDrawList* drawList, float sx, float sy, const char* text, ImU32 color)
{
	ImFont* font = ImGui::GetFont();
	float fontSize = ImGui::GetFontSize();
	drawList->AddText(font, fontSize, ImVec2(sx + 1.0f, sy + 1.0f), IM_COL32_BLACK, text);
	drawList->AddText(font, fontSize, ImVec2(sx, sy), color, text);
}

static void
Render2dfxLabelAtScreen(ImDrawList* drawList, float sx, float sy, Effect* e)
{
	const ImU32 colCyan = IM_COL32(0, 255, 255, 255);
	const ImU32 colGreen = IM_COL32(0, 255, 0, 255);
	const ImU32 colYellow = IM_COL32(255, 255, 0, 255);
	const ImU32 colWhite = IM_COL32(255, 255, 255, 255);
	const ImU32 colOrange = IM_COL32(255, 165, 0, 255);
	const ImU32 colPink = IM_COL32(255, 105, 180, 255);
	const ImU32 colPurple = IM_COL32(200, 100, 255, 255);

	char buf[128];
	float lineH = 14.0f;
	const char* typeName = Effects::GetEffectTypeName(e->type);
	float y = sy;

	switch(e->type){
	case FX_LIGHT:
		snprintf(buf, sizeof(buf), "Light [%d]", e->id);
		RenderLabelLine(drawList, sx, y, buf, colCyan); y += lineH;
		snprintf(buf, sizeof(buf), "Tex:%.12s", e->light.coronaTex);
		RenderLabelLine(drawList, sx, y, buf, colCyan); y += lineH;
		snprintf(buf, sizeof(buf), "Corona:%.1f Shadow:%.1f", e->light.coronaSize, e->light.shadowSize);
		RenderLabelLine(drawList, sx, y, buf, colCyan); y += lineH;
		snprintf(buf, sizeof(buf), "Flash:%d LODDist:%.0f", e->light.flashiness, e->light.lodDist);
		RenderLabelLine(drawList, sx, y, buf, colCyan); y += lineH;
		snprintf(buf, sizeof(buf), "Flare:%d Reflect:%d", e->light.lensFlareType, e->light.reflection);
		RenderLabelLine(drawList, sx, y, buf, colCyan); y += lineH;
		snprintf(buf, sizeof(buf), "ShdAlpha:%d Flags:0x%X", e->light.shadowAlpha, e->light.flags);
		RenderLabelLine(drawList, sx, y, buf, colCyan);
		break;

	case FX_PARTICLE:
		snprintf(buf, sizeof(buf), "Particle [%d]", e->id);
		RenderLabelLine(drawList, sx, y, buf, colPurple); y += lineH;
		snprintf(buf, sizeof(buf), "Name:%.20s", e->prtcl.name);
		RenderLabelLine(drawList, sx, y, buf, colPurple); y += lineH;
		snprintf(buf, sizeof(buf), "Type:%d Size:%.1f", e->prtcl.particleType, e->prtcl.size);
		RenderLabelLine(drawList, sx, y, buf, colPurple); y += lineH;
		snprintf(buf, sizeof(buf), "Dir:(%.2f,%.2f,%.2f)", e->prtcl.dir.x, e->prtcl.dir.y, e->prtcl.dir.z);
		RenderLabelLine(drawList, sx, y, buf, colPurple);
		break;

	case FX_LOOKATPOINT:
		snprintf(buf, sizeof(buf), "LookAt [%d]", e->id);
		RenderLabelLine(drawList, sx, y, buf, colPink); y += lineH;
		snprintf(buf, sizeof(buf), "Type:%d Prob:%d", e->look.type, e->look.probability);
		RenderLabelLine(drawList, sx, y, buf, colPink); y += lineH;
		snprintf(buf, sizeof(buf), "Dir:(%.2f,%.2f,%.2f)", e->look.dir.x, e->look.dir.y, e->look.dir.z);
		RenderLabelLine(drawList, sx, y, buf, colPink);
		break;

	case FX_PEDQUEUE:
		snprintf(buf, sizeof(buf), "PedQueue [%d]", e->id);
		RenderLabelLine(drawList, sx, y, buf, colYellow); y += lineH;
		snprintf(buf, sizeof(buf), "Script:%.6s Type:%d", e->queue.scriptName, e->queue.type);
		RenderLabelLine(drawList, sx, y, buf, colYellow); y += lineH;
		snprintf(buf, sizeof(buf), "Interest:%d LookAt:%d", e->queue.interest, e->queue.lookAt);
		RenderLabelLine(drawList, sx, y, buf, colYellow); y += lineH;
		snprintf(buf, sizeof(buf), "Flags:0x%X", e->queue.flags);
		RenderLabelLine(drawList, sx, y, buf, colYellow);
		break;

	case FX_INTERIOR:
		snprintf(buf, sizeof(buf), "Interior [%d]", e->id);
		RenderLabelLine(drawList, sx, y, buf, colGreen); y += lineH;
		snprintf(buf, sizeof(buf), "Group:%d Type:%d", e->interior.group, e->interior.type);
		RenderLabelLine(drawList, sx, y, buf, colGreen); y += lineH;
		snprintf(buf, sizeof(buf), "Size:%.1fx%.1fx%.1f", e->interior.width, e->interior.depth, e->interior.height);
		RenderLabelLine(drawList, sx, y, buf, colGreen); y += lineH;
		snprintf(buf, sizeof(buf), "Rot:%.1f", e->interior.rot);
		RenderLabelLine(drawList, sx, y, buf, colGreen);
		break;

	case FX_ENTRYEXIT:
		snprintf(buf, sizeof(buf), "EntryExit [%d]", e->id);
		RenderLabelLine(drawList, sx, y, buf, colOrange); y += lineH;
		snprintf(buf, sizeof(buf), "Title:%.6s Area:%d", e->entryExit.title, e->entryExit.areaCode);
		RenderLabelLine(drawList, sx, y, buf, colOrange); y += lineH;
		snprintf(buf, sizeof(buf), "Entry Ang:%.0f Exit Ang:%.0f", e->entryExit.enterAngle, e->entryExit.exitAngle);
		RenderLabelLine(drawList, sx, y, buf, colOrange); y += lineH;
		snprintf(buf, sizeof(buf), "Radius:%.1fx%.1f", e->entryExit.radiusX, e->entryExit.radiusY);
		RenderLabelLine(drawList, sx, y, buf, colOrange); y += lineH;
		snprintf(buf, sizeof(buf), "Open:%d Shut:%d", e->entryExit.openTime, e->entryExit.shutTime);
		RenderLabelLine(drawList, sx, y, buf, colOrange); y += lineH;
		snprintf(buf, sizeof(buf), "Extra:0x%X ExCol:%d", e->entryExit.extraFlags, e->entryExit.extraColor);
		RenderLabelLine(drawList, sx, y, buf, colOrange);
		break;

	case FX_ROADSIGN:
		snprintf(buf, sizeof(buf), "Roadsign [%d]", e->id);
		RenderLabelLine(drawList, sx, y, buf, colGreen); y += lineH;
		snprintf(buf, sizeof(buf), "Size:%.1fx%.1f", e->roadsign.width, e->roadsign.height);
		RenderLabelLine(drawList, sx, y, buf, colGreen); y += lineH;
		snprintf(buf, sizeof(buf), "Rot:(%.1f,%.1f,%.1f)", e->roadsign.rotX, e->roadsign.rotY, e->roadsign.rotZ);
		RenderLabelLine(drawList, sx, y, buf, colGreen); y += lineH;
		snprintf(buf, sizeof(buf), "Flags:0x%X", e->roadsign.flags);
		RenderLabelLine(drawList, sx, y, buf, colGreen); y += lineH;
		snprintf(buf, sizeof(buf), "Text:%.12s", e->roadsign.text[0]);
		RenderLabelLine(drawList, sx, y, buf, colGreen);
		break;

	case FX_TRIGGERPOINT:
		snprintf(buf, sizeof(buf), "Trigger [%d]", e->id);
		RenderLabelLine(drawList, sx, y, buf, colWhite); y += lineH;
		snprintf(buf, sizeof(buf), "Index:%d", e->triggerPoint.index);
		RenderLabelLine(drawList, sx, y, buf, colWhite);
		break;

	case FX_COVERPOINT:
		snprintf(buf, sizeof(buf), "Cover [%d]", e->id);
		RenderLabelLine(drawList, sx, y, buf, colCyan); y += lineH;
		snprintf(buf, sizeof(buf), "Dir:(%.2f,%.2f)", e->coverPoint.dirX, e->coverPoint.dirY);
		RenderLabelLine(drawList, sx, y, buf, colCyan); y += lineH;
		snprintf(buf, sizeof(buf), "Usage:%d", e->coverPoint.usage);
		RenderLabelLine(drawList, sx, y, buf, colCyan);
		break;

	case FX_ESCALATOR:
		snprintf(buf, sizeof(buf), "Escalator [%d]", e->id);
		RenderLabelLine(drawList, sx, y, buf, colPink); y += lineH;
		snprintf(buf, sizeof(buf), "GoingUp:%s", e->escalator.goingUp ? "Yes" : "No");
		RenderLabelLine(drawList, sx, y, buf, colPink); y += lineH;
		snprintf(buf, sizeof(buf), "Start:(%.1f,%.1f,%.1f)", e->escalator.bottom.x, e->escalator.bottom.y, e->escalator.bottom.z);
		RenderLabelLine(drawList, sx, y, buf, colPink); y += lineH;
		snprintf(buf, sizeof(buf), "End:(%.1f,%.1f,%.1f)", e->escalator.end.x, e->escalator.end.y, e->escalator.end.z);
		RenderLabelLine(drawList, sx, y, buf, colPink);
		break;

	default:
		snprintf(buf, sizeof(buf), "%s [%d]", typeName, e->id);
		RenderLabelLine(drawList, sx, y, buf, colWhite);
		break;
	}
}

void
RenderWorldLabels(void)
{
	if(!ImGui::GetCurrentContext())
		return;

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	if(drawList == nil)
		return;

	const ImU32 colYellow = IM_COL32(255, 255, 0, 255);
	const ImU32 colBlue = IM_COL32(100, 150, 255, 255);
	const ImU32 colGreen = IM_COL32(100, 255, 100, 255);
	const ImU32 colOrange = IM_COL32(255, 180, 100, 255);

	for(CPtrNode *p = instances.first; p; p = p->next){
		ObjectInst *inst = (ObjectInst*)p->item;
		if(inst->m_isDeleted)
			continue;

		float dist = TheCamera.distanceTo(inst->m_translation);
		if(dist > gWorldLabelDrawDist)
			continue;

		rw::V3d screen;
		float w, h;
		if(!Sprite::CalcScreenCoors(inst->m_translation, &screen, &w, &h, true))
			continue;

		float x = screen.x;
		float y = screen.y;

		if(gRenderAreaIdLabels){
			char buf[64];
			snprintf(buf, sizeof(buf), "Area:%d", inst->m_area);
			RenderLabelAtScreen(drawList, x, y, buf, colYellow);
			y += 14.0f;
		}

		if(gRender2dfxLabels){
			ObjectDef *obj = GetObjectDef(inst->m_objectId);
			if(obj && obj->m_numEffects > 0){
				for(int i = 0; i < obj->m_numEffects; i++){
					Effect *e = Effects::GetEffect(obj->m_effectIndex + i);
					if(e == nil)
						continue;

					rw::V3d worldPos;
					rw::V3d::transformPoints(&worldPos, &e->pos, 1, &inst->m_matrix);

					float effDist = TheCamera.distanceTo(worldPos);
					if(effDist > gWorldLabelDrawDist)
						continue;

					rw::V3d effScreen;
					if(Sprite::CalcScreenCoors(worldPos, &effScreen, &w, &h, true)){
						Render2dfxLabelAtScreen(drawList, effScreen.x, effScreen.y, e);
					}
				}
			}
		}
	}

	if(gRenderMapZoneLabels){
		int n = Zones::GetNumMapZones();
		for(int i = 0; i < n; i++){
			Zones::ZoneLabelInfo info;
			if(!Zones::GetMapZone(i, &info))
				continue;
			float dist = TheCamera.distanceTo(info.center);
			if(dist > gWorldLabelDrawDist)
				continue;
			rw::V3d screen;
			float w, h;
			if(!Sprite::CalcScreenCoors(info.center, &screen, &w, &h, true))
				continue;
			char buf[128];
			snprintf(buf, sizeof(buf), "MapZone:%s Lvl:%d", info.name, info.level);
			RenderLabelAtScreen(drawList, screen.x, screen.y, buf, colBlue);
		}
	}

	if(gRenderNavigZoneLabels){
		int n = Zones::GetNumNavigZones();
		for(int i = 0; i < n; i++){
			Zones::ZoneLabelInfo info;
			if(!Zones::GetNavigZone(i, &info))
				continue;
			float dist = TheCamera.distanceTo(info.center);
			if(dist > gWorldLabelDrawDist)
				continue;
			rw::V3d screen;
			float w, h;
			if(!Sprite::CalcScreenCoors(info.center, &screen, &w, &h, true))
				continue;
			char buf[128];
			const char *typeName = "Navig";
			switch(info.type){
			case 0: typeName = "Navig0"; break;
			case 1: typeName = "Navig1"; break;
			case 2: typeName = "Info"; break;
			case 3: typeName = "MapZone"; break;
			}
			snprintf(buf, sizeof(buf), "%s:%s Lvl:%d", typeName, info.name, info.level);
			RenderLabelAtScreen(drawList, screen.x, screen.y, buf, colGreen);
		}
	}

	if(gRenderAttribZoneLabels){
		int n = Zones::GetNumAttribZones();
		for(int i = 0; i < n; i++){
			Zones::AttribZoneLabelInfo info;
			if(!Zones::GetAttribZone(i, &info))
				continue;
			float dist = TheCamera.distanceTo(info.center);
			if(dist > gWorldLabelDrawDist)
				continue;
			rw::V3d screen;
			float w, h;
			if(!Sprite::CalcScreenCoors(info.center, &screen, &w, &h, true))
				continue;
			char buf[128];
			snprintf(buf, sizeof(buf), "Attribz Flags:0x%X WLD:%d", info.attribs, info.wantedLevelDrop);
			RenderLabelAtScreen(drawList, screen.x, screen.y, buf, colOrange);
		}
	}
}
