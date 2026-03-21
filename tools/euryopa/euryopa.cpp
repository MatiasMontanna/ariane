#include "euryopa.h"

int gameversion;
int gameplatform;

Params params;

int gGizmoMode = GIZMO_TRANSLATE;
bool gGizmoEnabled = true;

int gameTxdSlot;

int currentHour = 12;
int currentMinute = 0;
int extraColours = -1;
int currentArea;

// Options

bool gRenderOnlyLod;
bool gRenderOnlyHD;
bool gRenderBackground = true;
bool gRenderWater = true;
bool gRenderPostFX = true;
bool gEnableFog = true;
bool gEnableTimecycleBoxes = true;
bool gUseBlurAmb = gRenderPostFX;
bool gOverrideBlurAmb = true;
bool gNoTimeCull;
bool gNoAreaCull;
bool gDoBackfaceCulling;	// init from params
bool gPlayAnimations = true;
bool gUseViewerCam;
bool gDrawTarget = true;

// non-rendering things
bool gRenderCollision;
bool gRenderZones;
bool gRenderMapZones;
bool gRenderNavigZones;
bool gRenderInfoZones;
bool gRenderCullZones;
bool gRenderAttribZones;
bool gRenderPedPaths;
bool gRenderCarPaths;
bool gRenderEffects;
bool gRenderTimecycleBoxes;

// SA postfx
int  gColourFilter;
bool gRadiosity;

// SA building pipe
int gBuildingPipeSwitch = PLATFORM_PS2;
float gDayNightBalance;
float gWetRoadEffect;

// Neo stuff
float gNeoLightMapStrength = 0.5f;

bool
IsHourInRange(int h1, int h2)
{
	if(h1 > h2)
		return currentHour >= h1 || currentHour < h2;
	else
		return currentHour >= h1 && currentHour < h2;
}

static WeatherInfo weathersIII[] = {
	{ "SUNNY", Weather::Sunny },
	{ "CLOUDY", 0 },
	{ "RAINY", 0 },
	{ "FOGGY", Weather::Foggy }
};
static WeatherInfo weathersVC[] = {
	{ "SUNNY", Weather::Sunny },
	{ "CLOUDY", 0 },
	{ "RAINY", 0 },
	{ "FOGGY", Weather::Foggy },
	{ "EXTRASUNNY", Weather::Sunny | Weather::Extrasunny },
	{ "HURRICANE", 0 },
	{ "EXTRACOLOURS", 0 }
};
static WeatherInfo weathersLCS[] = {
	{ "SUNNY", Weather::Sunny },
	{ "CLOUDY", 0 },
	{ "RAINY", 0 },
	{ "FOGGY", Weather::Foggy },
	{ "EXTRASUNNY", Weather::Sunny | Weather::Extrasunny },
	{ "HURRICANE", 0 },
	{ "EXTRACOLOURS", 0 },
	{ "SNOW", 0 }
};
static WeatherInfo weathersVCS[] = {
	{ "SUNNY", Weather::Sunny },
	{ "CLOUDY", 0 },
	{ "RAINY", 0 },
	{ "FOGGY", Weather::Foggy },
	{ "EXTRASUNNY", Weather::Sunny | Weather::Extrasunny },
	{ "HURRICANE", 0 },
	{ "EXTRACOLOURS", 0 },
	{ "ULTRASUNNY", Weather::Sunny | Weather::Extrasunny }
};
static WeatherInfo weathersSA[] = {
	{ "EXTRASUNNY LA", Weather::Sunny | Weather::Extrasunny },
	{ "SUNNY LA", Weather::Sunny },
	{ "EXTRASUNNY SMOG LA", Weather::Sunny | Weather::Extrasunny },
	{ "SUNNY SMOG LA", Weather::Sunny },
	{ "CLOUDY LA", 0 },
	{ "SUNNY SF", Weather::Sunny },
	{ "EXTRASUNNY SF", Weather::Sunny | Weather::Extrasunny },
	{ "CLOUDY SF", 0 },
	{ "RAINY SF", 0 },
	{ "FOGGY SF", Weather::Foggy },
	{ "SUNNY VEGAS", Weather::Sunny },
	{ "EXTRASUNNY VEGAS", Weather::Sunny | Weather::Extrasunny },
	{ "CLOUDY VEGAS", 0 },
	{ "EXTRASUNNY COUNTRYSIDE", Weather::Sunny | Weather::Extrasunny },
	{ "SUNNY COUNTRYSIDE", Weather::Sunny },
	{ "CLOUDY COUNTRYSIDE", 0 },
	{ "RAINY COUNTRYSIDE", 0 },
	{ "EXTRASUNNY DESERT", Weather::Sunny | Weather::Extrasunny },
	{ "SUNNY DESERT", Weather::Sunny },
	{ "SANDSTORM DESERT", Weather::Foggy },
	{ "UNDERWATER", 0 },
	{ "EXTRACOLOURS 1", 0 },
	{ "EXTRACOLOURS 2", 0 }
};

void
InitParams(void)
{
	static const char *areasVC[] = {
		"Main Map", "Hotel", "Mansion", "Bank", "Mall", "Strip club",
		"Lawyer", "Coffee shop", "Concert hall", "Studio", "Rifle range",
		"Biker bar", "Police station", "Everywhere", "Dirt", "Blood", "Oval ring",
		"Malibu", "Print works"
	};

	params.initcampos.set(1356.0f, -1107.0f, 96.0f);
	params.initcamtarg.set(1276.0f, -984.0f, 68.0f);
	params.backfaceCull = true;
	params.alphaRefDefault = 2;
	params.alphaRef = 2;
	params.ps2AlphaTest = gameplatform == PLATFORM_PS2;
	params.map = gameversion;

	switch(gameversion){
	case GAME_III:
		params.initcampos.set(970.8f, -497.3f, 36.8f);
		params.initcamtarg.set(1092.5f, -417.3f, 3.8f);
		params.objFlagset = GAME_III;
		params.timecycle = GAME_III;
		params.numHours = 24;
		params.numWeathers = 4;
		params.weatherInfo = weathersIII;
		params.water = GAME_III;
		params.waterTex = "water_old";
		params.waterStart.set(-2048.0f, -2048.0f);
		params.waterEnd.set(2048.0f, 2048.0f);
		params.backfaceCull = false;
		params.checkColModels = true;
		params.maxNumColBoxes = 32;
		params.maxNumColSpheres = 128;
		params.maxNumColTriangles = 600;
		switch(gameplatform){
		case PLATFORM_PS2:
			break;
		case PLATFORM_PC:
			break;
		case PLATFORM_XBOX:
			// not so sure about the values
			// I think it's hardcoded by ID
			params.alphaRefDefault = 6;
			params.alphaRef = 128;
			params.txdFallbackGeneric = true;
			params.neoWorldPipe = GAME_III;
			break;
		}
		break;
	case GAME_VC:
		params.initcampos.set(131.5f, -1674.2f, 59.8f);
		params.initcamtarg.set(67.9f, -1542.0f, 26.3f);
		params.objFlagset = GAME_VC;
		params.numAreas = 19;
		params.areaNames = areasVC;
		params.timecycle = GAME_VC;
		params.numHours = 24;
		params.numWeathers = 7;
		params.extraColours = 6;
		params.numExtraColours = 1;
		params.weatherInfo = weathersVC;
		params.water = GAME_VC;
		params.waterTex = "waterclear256";
		params.waterStart.set(-2048.0f - 400.0f, -2048.0f);
		params.waterEnd.set(2048.0f - 400.0f, 2048.0f);
		switch(gameplatform){
		case PLATFORM_PS2:
			params.backfaceCull = false;
			break;
		case PLATFORM_PC:
			break;
		case PLATFORM_XBOX:
			// not so sure about the values
			// I think it's hardcoded by ID
			params.alphaRefDefault = 6;
			params.alphaRef = 128;
			params.neoWorldPipe = GAME_VC;
			params.backfaceCull = false;
			break;
		}
		break;
	case GAME_SA:
		params.initcampos.set(1789.0f, -1667.4f, 66.4f);
		params.initcamtarg.set(1679.1f, -1569.4f, 41.5f);
		params.objFlagset = GAME_SA;
		params.numAreas = 19;
		params.areaNames = areasVC;
		params.timecycle = GAME_SA;
		params.numHours = 8;
		params.numWeathers = 23;
		params.extraColours = 21;
		params.numExtraColours = 2;
		params.weatherInfo = weathersSA;
		params.background = GAME_SA;
		params.daynightPipe = true;
		params.water = GAME_SA;
		params.waterTex = "waterclear256";

		gBuildingPipeSwitch = gameplatform;
		gColourFilter = PLATFORM_PC;
		gRadiosity = gColourFilter == PLATFORM_PS2;
		if(gameplatform == PLATFORM_PS2){
			gColourFilter = PLATFORM_PS2;
		}else{
			params.alphaRefDefault = 2;
			params.alphaRef = 100;
		}
		break;
	case GAME_LCS:
		// TODO
		params.map = GAME_III;
		params.initcampos.set(970.8f, -497.3f, 36.8f);
		params.initcamtarg.set(1092.5f, -417.3f, 3.8f);
		params.objFlagset = GAME_VC;
		params.numAreas = 19;
		params.areaNames = areasVC;
		params.timecycle = GAME_LCS;
		params.numHours = 24;
		params.numWeathers = 8;
		params.extraColours = 6;
		params.numExtraColours = 1;
		params.weatherInfo = weathersLCS;
		params.water = GAME_III;
		params.waterTex = "waterclear256";
		params.waterStart.set(-2048.0f, -2048.0f);
		params.waterEnd.set(2048.0f, 2048.0f);
		params.backfaceCull = false;
		params.ps2AlphaTest = true;

		params.leedsPipe = 1;
		gBuildingPipeSwitch = PLATFORM_PS2;
		break;
	case GAME_VCS:
		// TODO
		params.map = GAME_VC;
		params.initcampos.set(131.5f, -1674.2f, 59.8f);
		params.initcamtarg.set(67.9f, -1542.0f, 26.3f);
		params.objFlagset = GAME_VC;
		params.numAreas = 19;
		params.areaNames = areasVC;
		params.timecycle = GAME_VCS;
		params.numHours = 24;
		params.numWeathers = 8;
		params.extraColours = 7;
		params.numExtraColours = 1;
		params.weatherInfo = weathersVCS;
		params.water = GAME_VC;
		params.waterTex = "waterclear256";
		params.waterStart.set(-2048.0f - 400.0f, -2048.0f);
		params.waterEnd.set(2048.0f - 400.0f, 2048.0f);
		params.backfaceCull = false;
		params.ps2AlphaTest = true;

		params.leedsPipe = 1;
		gBuildingPipeSwitch = PLATFORM_PS2;
		TheCamera.m_LODmult = 1.5f;
		break;
	// more configs in the future (LCSPC, VCSPC, UG, ...)
	}

	if(params.ps2AlphaTest){
		params.alphaRefDefault = 128;
		params.alphaRef = 128;
	}
}

void
FindVersion(void)
{
	FILE *f;

	if(f = fopen_ci("data/gta3.dat", "r"), f)
		gameversion = GAME_III;
	// This is wrong of course, but we'll use it as a hack
	else if(f = fopen_ci("data/gta_lcs.dat", "r"), f)
		gameversion = GAME_LCS;
	else if(f = fopen_ci("data/gta_vc.dat", "r"), f)
		gameversion = GAME_VC;
	else if(f = fopen_ci("data/gta_vcs.dat", "r"), f)
		gameversion = GAME_VCS;
	else if(f = fopen_ci("data/gta.dat", "r"), f)
		gameversion = GAME_SA;
	else{
		gameversion = GAME_NA;
		return;
	}
	if(doesFileExist("SYSTEM.CNF"))
		gameplatform = PLATFORM_PS2;
	else if(doesFileExist("default.xbe"))
		gameplatform = PLATFORM_XBOX;
	else
		gameplatform = PLATFORM_PC;
	fclose(f);
}

/*
void
test(void)
{
	CPtrNode *p;
	ObjectInst *inst, *inst2;
	int i;
	for(p = instances.first; p; p = p->next){
		inst = (ObjectInst*)p->item;

//		if(inst->m_numChildren > 1)
//			printf("%s has %d lod children\n", GetObjectDef(inst->m_objectId)->m_name, inst->m_numChildren);
		i = 0;
		for(inst2 = inst; inst2; inst2 = inst2->m_lod)
			i++;
		if(i > 2){
			printf("%s has %d lod levels\n", GetObjectDef(inst->m_objectId)->m_name, i);
			for(inst2 = inst; inst2; inst2 = inst2->m_lod)
				printf(" %s\n", GetObjectDef(inst2->m_objectId)->m_name);
		}
	}
	if(0){
		int i;
		CPtrNode *p;
		i = 0;
		for(p = instances.first; p; p = p->next)
			i++;
		log("%d instances\n", i);
	}
}*/

void
RenderEverythingColourCoded(void)
{
	rw::SetRenderState(rw::FOGENABLE, 0);
	SetRenderState(rw::ALPHATESTREF, 10);
	int aref = params.alphaRef;
	params.alphaRef = 10;
	gta::renderColourCoded = 1;
	RenderEverything();
	gta::renderColourCoded = 0;
	params.alphaRef = aref;
}

int32
pick(void)
{
	static rw::RGBA black = { 0, 0, 0, 0xFF };
	TheCamera.m_rwcam->clear(&black, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);
	RenderEverythingColourCoded();
	return gta::GetColourCode(CPad::newMouseState.x, CPad::newMouseState.y);
}

static rw::V3d
GetPlacementPosition(void)
{
	rw::V3d origin = TheCamera.m_position;
	rw::V3d dir = normalize(TheCamera.m_mouseDir);

	// Intersect ray with horizontal plane at camera target height
	float planeZ = TheCamera.m_target.z;
	if(fabs(dir.z) < 0.001f)
		return add(origin, scale(dir, 50.0f));

	float t = (planeZ - origin.z) / dir.z;
	if(t < 1.0f) t = 50.0f;
	if(t > 5000.0f) t = 5000.0f;

	rw::V3d pos;
	pos.x = origin.x + dir.x * t;
	pos.y = origin.y + dir.y * t;
	pos.z = planeZ;
	return pos;
}

void
handleTool(void)
{
	// Place mode intercepts all clicks
	if(gPlaceMode){
		if(CPad::IsMButtonClicked(1)){
			rw::V3d pos = GetPlacementPosition();
			SpawnPlaceObject(pos);
			return;
		}
		if(CPad::IsMButtonClicked(2) || CPad::IsKeyJustDown(KEY_ESC)){
			SpawnExitPlaceMode();
			return;
		}
		return;  // Absorb clicks while in place mode
	}

	// select
	if(CPad::IsMButtonClicked(1)){
		ObjectInst *inst = GetInstanceByID(pick());
		if(inst && !inst->m_isDeleted){
			if(CPad::IsShiftDown())
				inst->Select();
			else if(CPad::IsAltDown())
				inst->Deselect();
			else if(CPad::IsCtrlDown()){
				if(inst->m_selected) inst->Deselect();
				else inst->Select();
			}else{
				ClearSelection();
				inst->Select();
			}
		}else
			ClearSelection();

		Path::selectedNode = Path::hoveredNode;
		Effects::selectedEffect = Effects::hoveredEffect;
	}else if(CPad::IsMButtonClicked(2)){
		if(CPad::IsCtrlDown()){
			Path::selectedNode = Path::hoveredNode;
			Effects::selectedEffect = Effects::hoveredEffect;
		}else{
			ClearSelection();
			ObjectInst *inst = GetInstanceByID(pick());
			if(inst && !inst->m_isDeleted)
				inst->Select();
		}
	}else if(CPad::IsMButtonClicked(3)){
		ClearSelection();
		Path::selectedNode = nil;
		Effects::selectedEffect = nil;
	}
}

rw::Texture *(*originalFindCB)(const char *name);
rw::TexDictionary *fallbackTxd;
static rw::Texture*
fallbackFindCB(const char *name)
{
	rw::Texture *t = originalFindCB(name);
	if(t) return t;
	return fallbackTxd->find(name);
}

void
LoadGame(void)
{
// for debugging...
//	SetCurrentDirectory("C:/Users/aap/games/gta3");
//	SetCurrentDirectory("C:/Users/aap/games/gtavc");
//	SetCurrentDirectory("C:/Users/aap/games/gtasa");

	FindVersion();
	switch(gameversion){
	case GAME_III: debug("found III!\n"); break;
	case GAME_VC: debug("found VC!\n"); break;
	case GAME_SA: debug("found SA!\n"); break;
	case GAME_LCS: debug("found LCS!\n"); break;
	case GAME_VCS: debug("found VCS!\n"); break;
	default: panic("unknown game");
	}
	switch(gameplatform){
	case PLATFORM_PS2: debug("assuming PS2\n"); break;
	case PLATFORM_XBOX: debug("assuming Xbox\n"); break;
	default: debug("assuming PC\n"); break;
	}
	InitParams();

	TheCamera.m_position = params.initcampos;
	TheCamera.m_target = params.initcamtarg;
	TheCamera.setDistanceToTarget(50.0f);
	gDoBackfaceCulling = params.backfaceCull;

	defaultTxd = rw::TexDictionary::getCurrent();

	int particleTxdSlot = AddTxdSlot("particle");
	LoadTxd(particleTxdSlot, "MODELS/PARTICLE.TXD");

	gameTxdSlot = AddTxdSlot("generic");
	CreateTxd(gameTxdSlot);
	TxdMakeCurrent(gameTxdSlot);
	if(params.txdFallbackGeneric){
		fallbackTxd = rw::TexDictionary::getCurrent();
		originalFindCB = rw::Texture::findCB;
		rw::Texture::findCB = fallbackFindCB;
	}

	Timecycle::Initialize();
	if(params.neoWorldPipe)
		Timecycle::InitNeoWorldTweak();
	WaterLevel::Initialise();
	Clouds::Init();

	AddColSlot("generic");
	AddIplSlot("generic");

	AddCdImage("MODELS\\GTA3.IMG");
	if(isSA())
		AddCdImage("MODELS\\GTA_INT.IMG");

	FileLoader::LoadLevel("data/default.dat");
	switch(gameversion){
	case GAME_III: FileLoader::LoadLevel("data/gta3.dat"); break;
	case GAME_VC: FileLoader::LoadLevel("data/gta_vc.dat"); break;
	case GAME_SA: FileLoader::LoadLevel("data/gta.dat"); break;
	case GAME_LCS: FileLoader::LoadLevel("data/gta_lcs.dat"); break;
	case GAME_VCS: FileLoader::LoadLevel("data/gta_vcs.dat"); break;
	}

	InitLodLookup();
	InitObjectCategories();
	LoadFavourites();
	// InitPreviewRenderer called lazily on first use
	InitSectors();

	CPtrNode *p;
	ObjectInst *inst;
	for(p = instances.first; p; p = p->next){
		inst = (ObjectInst*)p->item;
		InsertInstIntoSectors(inst);
	}

	// hide the islands
	ObjectDef *obj;
	if(params.map == GAME_III){
		obj = GetObjectDef("IslandLODInd", nil);
		if(obj) obj->m_isHidden = true;
		obj = GetObjectDef("IslandLODcomIND", nil);
		if(obj) obj->m_isHidden = true;
		obj = GetObjectDef("IslandLODcomSUB", nil);
		if(obj) obj->m_isHidden = true;
		obj = GetObjectDef("IslandLODsubIND", nil);
		if(obj) obj->m_isHidden = true;
		obj = GetObjectDef("IslandLODsubCOM", nil);
		if(obj) obj->m_isHidden = true;
	}else if(params.map == GAME_VC){
		obj = GetObjectDef("IslandLODmainland", nil);
		if(obj) obj->m_isHidden = true;
		obj = GetObjectDef("IslandLODbeach", nil);
		if(obj) obj->m_isHidden = true;
	}
}

static void
updateRwFrame(ObjectInst *inst)
{
	if(inst->m_rwObject == nil) return;
	ObjectDef *obj = GetObjectDef(inst->m_objectId);
	rw::Frame *f;
	if(obj->m_type == ObjectDef::ATOMIC)
		f = ((rw::Atomic*)inst->m_rwObject)->getFrame();
	else
		f = ((rw::Clump*)inst->m_rwObject)->getFrame();
	f->transform(&inst->m_matrix, rw::COMBINEREPLACE);
}

void
dogizmo(void)
{
	if(!gGizmoEnabled)
		return;
	if(!selection.first)
		return;

	ObjectInst *inst = (ObjectInst*)selection.first->item;
	if(inst->m_isDeleted)
		return;

	static bool wasDragging = false;
	static rw::V3d dragStartPos;
	static rw::Quat dragStartRot;
	static rw::V3d dragStartLodPos;
	static ObjectInst *dragLodInst;

	rw::Camera *cam;
	rw::RawMatrix gizobj;
	float *fview, *fproj, *fobj;

	// Build object matrix from instance
	rw::convMatrix(&gizobj, &inst->m_matrix);

	cam = (rw::Camera*)rw::engine->currentCamera;
	fview = (float*)&cam->devView;
	fproj = (float*)&cam->devProj;
	fobj = (float*)&gizobj;

	ImGuiIO &io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

	ImGuizmo::OPERATION op = gGizmoMode == GIZMO_ROTATE ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE;
	ImGuizmo::Manipulate(fview, fproj, op, ImGuizmo::LOCAL, fobj, nil, nil);

	bool isUsing = ImGuizmo::IsUsing();

	// Capture start state when drag begins
	if(isUsing && !wasDragging){
		dragStartPos = inst->m_translation;
		dragStartRot = inst->m_rotation;
		dragLodInst = inst->m_lod;
		if(dragLodInst)
			dragStartLodPos = dragLodInst->m_translation;
	}
	// Record undo when drag ends
	if(!isUsing && wasDragging){
		if(gGizmoMode == GIZMO_TRANSLATE)
			UndoRecordMove(inst, dragStartPos, dragLodInst, dragStartLodPos);
		else if(gGizmoMode == GIZMO_ROTATE)
			UndoRecordRotate(inst, dragStartRot);
	}
	wasDragging = isUsing;

	if(isUsing){
		// Extract translation from the gizmo result
		rw::V3d newPos;
		newPos.x = gizobj.pos.x;
		newPos.y = gizobj.pos.y;
		newPos.z = gizobj.pos.z;

		if(gGizmoMode == GIZMO_TRANSLATE){
			// Compute delta
			rw::V3d delta = sub(newPos, inst->m_translation);

			inst->m_translation = newPos;
			inst->m_isDirty = true;
			inst->UpdateMatrix();
			updateRwFrame(inst);

			// Also move the LOD if this instance has one
			if(inst->m_lod && !inst->m_lod->m_isDeleted){
				inst->m_lod->m_translation = add(inst->m_lod->m_translation, delta);
				inst->m_lod->m_isDirty = true;
				inst->m_lod->UpdateMatrix();
				updateRwFrame(inst->m_lod);
			}
			// If this IS a LOD, move all HD children that reference it
			CPtrNode *p;
			for(p = instances.first; p; p = p->next){
				ObjectInst *other = (ObjectInst*)p->item;
				if(other != inst && other->m_lod == inst && !other->m_isDeleted){
					other->m_translation = add(other->m_translation, delta);
					other->m_isDirty = true;
					other->UpdateMatrix();
					updateRwFrame(other);
				}
			}
		}else if(gGizmoMode == GIZMO_ROTATE){
			// Extract rotation from the RawMatrix gizmo result
			// Copy rotation part, keep same translation
			inst->m_matrix.right.x = gizobj.right.x;
			inst->m_matrix.right.y = gizobj.right.y;
			inst->m_matrix.right.z = gizobj.right.z;
			inst->m_matrix.up.x = gizobj.up.x;
			inst->m_matrix.up.y = gizobj.up.y;
			inst->m_matrix.up.z = gizobj.up.z;
			inst->m_matrix.at.x = gizobj.at.x;
			inst->m_matrix.at.y = gizobj.at.y;
			inst->m_matrix.at.z = gizobj.at.z;
			// pos stays the same

			// Extract quaternion from the rotation matrix
			// Matrix was built with conj(m_rotation), so we extract
			// the quat from the matrix and conjugate it (negate x,y,z)
			rw::Quat q;
			rw::Matrix *m = &inst->m_matrix;
			float trace = m->right.x + m->up.y + m->at.z;
			if(trace > 0.0f){
				float s = sqrtf(trace + 1.0f) * 2.0f;
				q.w = 0.25f * s;
				q.x = (m->up.z - m->at.y) / s;
				q.y = (m->at.x - m->right.z) / s;
				q.z = (m->right.y - m->up.x) / s;
			}else if(m->right.x > m->up.y && m->right.x > m->at.z){
				float s = sqrtf(1.0f + m->right.x - m->up.y - m->at.z) * 2.0f;
				q.w = (m->up.z - m->at.y) / s;
				q.x = 0.25f * s;
				q.y = (m->up.x + m->right.y) / s;
				q.z = (m->at.x + m->right.z) / s;
			}else if(m->up.y > m->at.z){
				float s = sqrtf(1.0f + m->up.y - m->right.x - m->at.z) * 2.0f;
				q.w = (m->at.x - m->right.z) / s;
				q.x = (m->up.x + m->right.y) / s;
				q.y = 0.25f * s;
				q.z = (m->at.y + m->up.z) / s;
			}else{
				float s = sqrtf(1.0f + m->at.z - m->right.x - m->up.y) * 2.0f;
				q.w = (m->right.y - m->up.x) / s;
				q.x = (m->at.x + m->right.z) / s;
				q.y = (m->at.y + m->up.z) / s;
				q.z = 0.25f * s;
			}
			// Conjugate: matrix was built from conj(rotation)
			inst->m_rotation.x = -q.x;
			inst->m_rotation.y = -q.y;
			inst->m_rotation.z = -q.z;
			inst->m_rotation.w = q.w;
			inst->m_isDirty = true;
		}

		// Update the rw object frame for rotation case
		if(gGizmoMode == GIZMO_ROTATE)
			updateRwFrame(inst);
	}
}

static uint64 frameCounter;

void
updateFPS(void)
{
	static float history[100];
	static float total;
	static int n;
	static int i;

	total += timeStep - history[i];
	history[i] = timeStep;
	i = (i+1) % 100;
	n = i > n ? i : n;
	avgTimeStep = total / n;
}

void
Draw(void)
{
	static rw::RGBA clearcol = { 0x80, 0x80, 0x80, 0xFF };

	CPad *pad = CPad::GetPad(0);
	if(pad->NewState.start && pad->NewState.select){
		sk::globals.quit = 1;
		return;
	}

	// HACK: we load a lot in the first frame
	// which messes up the average
	if(frameCounter == 0)
		timeStep = 1/30.0f;

	if(!gOverrideBlurAmb)
		gUseBlurAmb = gRenderPostFX;

	updateFPS();

	Weather::Update();
	Timecycle::Update();
	Timecycle::SetLights();

	UpdateDayNightBalance();

	TheCamera.m_rwcam->setFarPlane(Timecycle::currentColours.farClp);
	TheCamera.m_rwcam->fogPlane = Timecycle::currentColours.fogSt;
	TheCamera.m_rwcam_viewer->setFarPlane(5000.0f);
	TheCamera.m_rwcam_viewer->fogPlane = Timecycle::currentColours.fogSt;

	CPad::UpdatePads();
	TheCamera.Process();
	TheCamera.update();
	if(gUseViewerCam)
		Scene.camera = TheCamera.m_rwcam_viewer;
	else
		Scene.camera = TheCamera.m_rwcam;
	// Render 3D preview to texture (before main camera update)
	if(GetSpawnObjectId() >= 0)
		RenderPreviewObject(GetSpawnObjectId());

	Scene.camera->beginUpdate();

	DefinedState();

	ImGui_ImplRW_NewFrame(timeStep);
	ImGuizmo::BeginFrame();

	LoadAllRequestedObjects();
	BuildRenderList();

	// Has to be called for highlighting some objects
	// but also can mess with timecycle mid frame :/
	gui();

	dogizmo();

	handleTool();

	DefinedState();
	Scene.camera->clear(&clearcol, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);
	if(gRenderBackground){
		SetRenderState(rw::ALPHATESTREF, 0);
		SetRenderState(rw::CULLMODE, rw::CULLNONE);
		rw::RGBA skytop, skybot;
		rw::convColor(&skytop, &Timecycle::currentColours.skyTop);
		rw::convColor(&skybot, &Timecycle::currentColours.skyBottom);
		if(params.background == GAME_SA){
			Clouds::RenderSkyPolys();
			Clouds::RenderLowClouds();
		}else{
			Clouds::RenderBackground(skytop.red, skytop.green, skytop.blue,
				skybot.red, skybot.green, skybot.blue, 255);
			Clouds::RenderLowClouds();
			if(params.timecycle != GAME_VCS)
				Clouds::RenderFluffyClouds();
			Clouds::RenderHorizon();
		}
		SetRenderState(rw::ALPHATESTREF, params.alphaRef);
	}

	rw::SetRenderState(rw::FOGENABLE, gEnableFog);
	RenderOpaque();
	if(gRenderWater)
		WaterLevel::Render();
	RenderTransparent();
	// DEBUG render object picking
	//RenderEverythingColourCoded();


	if(gRenderPostFX)
		RenderPostFX();

	DefinedState();
	rw::SetRenderState(rw::FOGENABLE, 0);

	SetRenderState(rw::CULLMODE, rw::CULLNONE);

	if(gDrawTarget)
		TheCamera.DrawTarget();
	if(gRenderCollision)
		RenderEverythingCollisions();
	if(gRenderTimecycleBoxes)
		Timecycle::RenderBoxes();
	if(gRenderZones)
		Zones::Render();
	if(gRenderCullZones)
		Zones::RenderCullZones();
	if(gRenderAttribZones)
		Zones::RenderAttribZones();
	Path::hoveredNode = nil;
	Effects::hoveredEffect = nil;
	if(gRenderPedPaths)
		Path::RenderPedPaths();
	if(gRenderCarPaths)
		Path::RenderCarPaths();
	if(gRenderEffects)
		Effects::Render();

	rw::SetRenderState(rw::ALPHATESTFUNC, rw::ALPHAALWAYS);	// don't mess up GUI
	// This fucks up the z buffer, but what else can we do?
	RenderDebugLines();
	ImGui::EndFrame();
	ImGui::Render();

	ImGui_ImplRW_RenderDrawLists(ImGui::GetDrawData());

	Scene.camera->endUpdate();
	Scene.camera->showRaster(rw::Raster::FLIPWAITVSYNCH);
	frameCounter++;
}

void
Idle(void)
{
	static int state = 0;
	switch(state){
	case 0:
		LoadGame();
		state = 1;
		break;
	case 1:
		Draw();
		break;
	}
}
