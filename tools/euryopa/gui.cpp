#include "euryopa.h"
#include "autocol.h"
#include "modloader.h"
#include "imgui/imgui_internal.h"
#include "object_categories.h"
#include "telemetry.h"
#include "updater.h"
#include "icons.h"
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#else
#include <dirent.h>
#endif

static bool showDemoWindow;
static bool showEditorWindow;
static bool showInstanceWindow;
static bool showLogWindow;
static bool showHelpWindow;

static bool showTimeWeatherWindow;
static bool showViewWindow;
static bool showRenderingWindow;
static bool showBrowserWindow;
static bool showDiffWindow;
static bool showToolsWindow = true;
static bool gSaNodeJustSelected;
static bool gBrowserIdeListDirty = true;
static char gIplFilterSearch[128];
static char gEditorCameraName[256] = "default";

static bool gAutomaticBackupsEnabled = true;
static int gAutomaticBackupIntervalSeconds = 300;
static int gAutomaticBackupKeepCount = 10;
static int gCustomImportPreferredStartId = 18631;
static const float gAutomaticBackupIdleSeconds = 5.0f;
static float gAutomaticBackupSecondsSinceLastRun = 0.0f;
static float gAutomaticBackupSecondsSinceLastChange = 0.0f;
static uint32 gAutomaticBackupLastSeenSeq = 0;
static uint32 gAutomaticBackupLastHandledSeq = 0;
static char gAutomaticBackupLastSnapshot[1024];

static ImGuiTextFilter gEditorModelFilter;
static ImGuiTextFilter gEditorTxdFilter;
static bool gEditorHighlightMatches;

static ImGuiTextFilter gBrowserCategoryFilter;
static ImGuiTextFilter gBrowserIdeFilter;
static ImGuiTextFilter gBrowserSearchFilter;
static ImGuiTextFilter gBrowserFavFilter;
static int gBrowserSelectedCategory = -1;
static char gBrowserSelectedIde[256];
static bool gBrowserTabRestorePending;
static int gDiffFilter;
static int gRenderMode;

enum BrowserTabId
{
	BROWSER_TAB_CATEGORIES,
	BROWSER_TAB_IDE,
	BROWSER_TAB_SEARCH,
	BROWSER_TAB_FAVOURITES
};
static int gBrowserActiveTab = BROWSER_TAB_CATEGORIES;

struct SavedIplVisibilityState
{
	char key[256];
	bool visible;
};
static std::vector<SavedIplVisibilityState> gSavedIplVisibilityStates;

static bool gSavedWindowStateLoaded;
static bool gSavedWindowPlacementValid;
static bool gSavedWindowPlacementApplied;
static int gSavedWindowX;
static int gSavedWindowY;
static int gSavedWindowWidth = 1280;
static int gSavedWindowHeight = 800;
static bool gSavedWindowMaximized;
static float gSettingsAutosaveSeconds;
static bool gPersistentSettingsLoaded;

static void loadSaveSettings(void);
static void saveSaveSettings(void);
static void normalizePersistentSettings(void);

static int
getDefaultCustomImportStartId(void)
{
	return isSA() ? 18631 : 0;
}

static void
sanitizeCustomImportSettings(void)
{
	if(gCustomImportPreferredStartId < 0 || gCustomImportPreferredStartId >= NUMOBJECTDEFS)
		gCustomImportPreferredStartId = getDefaultCustomImportStartId();
}

static bool
getEditorRootDirectory(char *dir, size_t size)
{
	if(size == 0)
		return false;

#ifdef _WIN32
	DWORD len = GetModuleFileNameA(nil, dir, (DWORD)size);
	if(len > 0 && len < size){
		for(int i = (int)len - 1; i >= 0; i--){
			if(dir[i] == '\\' || dir[i] == '/'){
				dir[i] = '\0';
				return true;
			}
		}
	}

	len = GetCurrentDirectoryA((DWORD)size, dir);
	return len > 0 && len < size;
#else
	strncpy(dir, ".", size);
	dir[size - 1] = '\0';
	return true;
#endif
}

static bool
buildPath(char *dst, size_t size, const char *dir, const char *name)
{
	if(size == 0)
		return false;
	if(dir == nil || dir[0] == '\0')
		return snprintf(dst, size, "%s", name) < (int)size;

	size_t len = strlen(dir);
#ifdef _WIN32
	const char *sep = (dir[len-1] == '\\' || dir[len-1] == '/') ? "" : "\\";
#else
	const char *sep = (dir[len-1] == '/') ? "" : "/";
#endif
	return snprintf(dst, size, "%s%s%s", dir, sep, name) < (int)size;
}

static bool
getLegacyRootPath(char *dst, size_t size, const char *name)
{
	char rootDir[2048];
	return getEditorRootDirectory(rootDir, sizeof(rootDir)) &&
	       buildPath(dst, size, rootDir, name);
}

static const char*
skipSpaces(const char *p)
{
	while(*p && isspace((unsigned char)*p))
		p++;
	return p;
}

static bool
splitSettingLine(const char *line, char *key, size_t keySize, const char **value)
{
	size_t len;
	const char *p = skipSpaces(line);
	if(*p == '\0' || *p == '#')
		return false;

	len = 0;
	while(p[len] && !isspace((unsigned char)p[len]))
		len++;
	if(len == 0 || len >= keySize)
		return false;

	memcpy(key, p, len);
	key[len] = '\0';
	*value = skipSpaces(p + len);
	return true;
}

static bool
parseIntSetting(const char *value, int *out)
{
	return sscanf(skipSpaces(value), "%d", out) == 1;
}

static bool
parseFloatSetting(const char *value, float *out)
{
	return sscanf(skipSpaces(value), "%f", out) == 1;
}

static bool
parseBoolSetting(const char *value, bool *out)
{
	int i;
	if(!parseIntSetting(value, &i))
		return false;
	*out = i != 0;
	return true;
}

static bool
parseVec3Setting(const char *value, rw::V3d *out)
{
	return sscanf(skipSpaces(value), "%f %f %f", &out->x, &out->y, &out->z) == 3;
}

static bool
parseQuotedStringValue(const char *value, char *out, size_t outSize, const char **after = nil)
{
	size_t len = 0;
	const char *p = skipSpaces(value);
	if(*p != '"' || outSize == 0)
		return false;
	p++;

	while(*p && *p != '"'){
		char c = *p++;
		if(c == '\\'){
			c = *p++;
			if(c == '\0')
				return false;
			switch(c){
			case 'n': c = '\n'; break;
			case 'r': c = '\r'; break;
			case 't': c = '\t'; break;
			case '\\':
			case '"':
				break;
			default:
				break;
			}
		}
		if(len + 1 < outSize)
			out[len++] = c;
	}
	if(*p != '"')
		return false;

	out[len] = '\0';
	if(after)
		*after = p + 1;
	return true;
}

static void
writeQuotedSetting(FILE *f, const char *key, const char *value)
{
	fprintf(f, "%s \"", key);
	if(value){
		for(const char *p = value; *p; p++){
			switch(*p){
			case '\\':
			case '"':
				fputc('\\', f);
				fputc(*p, f);
				break;
			case '\n':
				fputs("\\n", f);
				break;
			case '\r':
				fputs("\\r", f);
				break;
			case '\t':
				fputs("\\t", f);
				break;
			default:
				fputc(*p, f);
				break;
			}
		}
	}
	fputs("\"\n", f);
}

static void
writeInlineQuotedString(FILE *f, const char *value)
{
	fputc('"', f);
	if(value){
		for(const char *p = value; *p; p++){
			switch(*p){
			case '\\':
			case '"':
				fputc('\\', f);
				fputc(*p, f);
				break;
			case '\n':
				fputs("\\n", f);
				break;
			case '\r':
				fputs("\\r", f);
				break;
			case '\t':
				fputs("\\t", f);
				break;
			default:
				fputc(*p, f);
				break;
			}
		}
	}
	fputc('"', f);
}

static void
setTextFilterValue(ImGuiTextFilter &filter, const char *value)
{
	if(value == nil || value[0] == '\0'){
		filter.Clear();
		return;
	}
	strncpy(filter.InputBuf, value, IM_ARRAYSIZE(filter.InputBuf)-1);
	filter.InputBuf[IM_ARRAYSIZE(filter.InputBuf)-1] = '\0';
	filter.Build();
}

static void
loadWindowStateFromSettingsFile(void)
{
	FILE *f;
	char line[512];
	char key[128];
	const char *value;
	bool haveWindowX = false;
	bool haveWindowY = false;

	if(gSavedWindowStateLoaded)
		return;
	gSavedWindowStateLoaded = true;

	f = fopenArianeDataRead("savesettings.txt", "savesettings.txt");
	if(f == nil)
		return;

	while(fgets(line, sizeof(line), f)){
		if(!splitSettingLine(line, key, sizeof(key), &value))
			continue;
		if(strcmp(key, "window_width") == 0){
			parseIntSetting(value, &gSavedWindowWidth);
		}else if(strcmp(key, "window_height") == 0){
			parseIntSetting(value, &gSavedWindowHeight);
		}else if(strcmp(key, "window_x") == 0){
			haveWindowX = parseIntSetting(value, &gSavedWindowX);
		}else if(strcmp(key, "window_y") == 0){
			haveWindowY = parseIntSetting(value, &gSavedWindowY);
		}else if(strcmp(key, "window_maximized") == 0){
			parseBoolSetting(value, &gSavedWindowMaximized);
		}
	}
	fclose(f);

	gSavedWindowWidth = clamp(gSavedWindowWidth, 640, 8192);
	gSavedWindowHeight = clamp(gSavedWindowHeight, 480, 8192);
	gSavedWindowPlacementValid = haveWindowX && haveWindowY;
}

void
LoadInitialEditorWindowState(int *width, int *height)
{
	loadWindowStateFromSettingsFile();
	if(width)
		*width = gSavedWindowWidth;
	if(height)
		*height = gSavedWindowHeight;
}

void
OnEditorWindowResized(int width, int height)
{
	gSavedWindowWidth = clamp(width, 1, 8192);
	gSavedWindowHeight = clamp(height, 1, 8192);
}

#ifdef _WIN32
static HWND
getEditorWindowHandle(void)
{
#ifdef RW_D3D9
	return (HWND)engineOpenParams.window;
#else
	return nil;
#endif
}
#endif

void
ApplyInitialEditorWindowState(void)
{
	loadWindowStateFromSettingsFile();
	if(gSavedWindowPlacementApplied)
		return;
	gSavedWindowPlacementApplied = true;

#ifdef _WIN32
	HWND hwnd = getEditorWindowHandle();
	if(hwnd && gSavedWindowPlacementValid){
		SetWindowPos(hwnd, nil, gSavedWindowX, gSavedWindowY,
			gSavedWindowWidth, gSavedWindowHeight,
			SWP_NOZORDER | SWP_NOACTIVATE);
		if(gSavedWindowMaximized)
			ShowWindow(hwnd, SW_MAXIMIZE);
	}
#endif
}

void
UpdateEditorWindowState(void)
{
#ifdef _WIN32
	HWND hwnd = getEditorWindowHandle();
	if(hwnd){
		WINDOWPLACEMENT placement;
		placement.length = sizeof(placement);
		if(GetWindowPlacement(hwnd, &placement)){
			RECT rect = placement.rcNormalPosition;
			if(placement.showCmd == SW_SHOWMAXIMIZED){
				gSavedWindowMaximized = true;
			}else if(placement.showCmd == SW_SHOWMINIMIZED || placement.showCmd == SW_MINIMIZE ||
			          placement.showCmd == SW_SHOWMINNOACTIVE){
				gSavedWindowMaximized = (placement.flags & WPF_RESTORETOMAXIMIZED) != 0;
			}else{
				GetWindowRect(hwnd, &rect);
				gSavedWindowMaximized = false;
			}

			gSavedWindowX = rect.left;
			gSavedWindowY = rect.top;
			gSavedWindowWidth = max((int)(rect.right - rect.left), 1);
			gSavedWindowHeight = max((int)(rect.bottom - rect.top), 1);
			gSavedWindowPlacementValid = true;
		}
	}
#endif
}

static void
normalizePersistentSettings(void)
{
	currentHour = ((currentHour % 24) + 24) % 24;
	currentMinute = ((currentMinute % 60) + 60) % 60;
	if(params.numAreas > 0)
		currentArea = clamp(currentArea, 0, params.numAreas-1);
	else
		currentArea = 0;
	if(params.numWeathers > 0){
		Weather::oldWeather = clamp(Weather::oldWeather, 0, params.numWeathers-1);
		Weather::newWeather = clamp(Weather::newWeather, 0, params.numWeathers-1);
	}else{
		Weather::oldWeather = 0;
		Weather::newWeather = 0;
	}
	if(params.timecycle != GAME_III && params.numExtraColours > 0 && params.numHours > 0)
		extraColours = clamp(extraColours, -1, params.numExtraColours*params.numHours - 1);
	else
		extraColours = -1;
	Weather::interpolation = clamp(Weather::interpolation, 0.0f, 1.0f);
	TheCamera.m_fov = clamp(TheCamera.m_fov, 1.0f, 150.0f);
	TheCamera.m_LODmult = clamp(TheCamera.m_LODmult, 0.5f, 3.0f);
	gNeoLightMapStrength = clamp(gNeoLightMapStrength, 0.0f, 1.0f);
	gDayNightBalance = clamp(gDayNightBalance, 0.0f, 1.0f);
	gWetRoadEffect = clamp(gWetRoadEffect, 0.0f, 1.0f);
	gSaPedPathWalkerCount = clamp(gSaPedPathWalkerCount, 1, 32);
	gSaCarPathTrafficCount = clamp(gSaCarPathTrafficCount, 1, 32);
	gSaCarPathTrafficSpeedScale = clamp(gSaCarPathTrafficSpeedScale, 0.25f, 3.0f);
	gSaCarPathParkedCarCount = clamp(gSaCarPathParkedCarCount, 1, 24);
	gRenderMode = clamp(gRenderMode, 0, 2);
	gRenderOnlyHD = gRenderMode == 1;
	gRenderOnlyLod = gRenderMode == 2;
	gGizmoMode = gGizmoMode == GIZMO_ROTATE ? GIZMO_ROTATE : GIZMO_TRANSLATE;
	gBrowserActiveTab = clamp(gBrowserActiveTab, (int)BROWSER_TAB_CATEGORIES, (int)BROWSER_TAB_FAVOURITES);
	gBrowserSelectedCategory = clamp(gBrowserSelectedCategory, -1, NUM_OBJ_CATEGORIES-1);
	gDiffFilter = max(gDiffFilter, 0);
	WaterLevel::gWaterSubMode = clamp(WaterLevel::gWaterSubMode, 0, 1);
	WaterLevel::gWaterCreateShape = clamp(WaterLevel::gWaterCreateShape, 0, 1);
	WaterLevel::gWaterSnapSize = clamp(WaterLevel::gWaterSnapSize, 1.0f, 100.0f);
	params.alphaRef = clamp(params.alphaRef, 0, 255);

	switch(params.timecycle){
	case GAME_SA:
		if(gColourFilter == PLATFORM_XBOX)
			gColourFilter = PLATFORM_PC;
		if(gColourFilter != 0 && gColourFilter != PLATFORM_PS2 &&
		   gColourFilter != PLATFORM_PC)
			gColourFilter = PLATFORM_PC;
		break;
	case GAME_LCS:
		gRadiosity = false;
		if(gColourFilter != 0 && gColourFilter != PLATFORM_PS2 &&
		   gColourFilter != PLATFORM_PSP)
			gColourFilter = 0;
		break;
	case GAME_VCS:
		if(gColourFilter != 0 && gColourFilter != PLATFORM_PS2 &&
		   gColourFilter != PLATFORM_PSP)
			gColourFilter = 0;
		break;
	default:
		gColourFilter = 0;
		gRadiosity = false;
		break;
	}

	if(params.daynightPipe){
		if(gBuildingPipeSwitch != PLATFORM_PS2 &&
		   gBuildingPipeSwitch != PLATFORM_PC &&
		   gBuildingPipeSwitch != PLATFORM_XBOX)
			gBuildingPipeSwitch = PLATFORM_PS2;
	}else if(params.leedsPipe){
		if(gBuildingPipeSwitch != PLATFORM_NULL &&
		   gBuildingPipeSwitch != PLATFORM_PSP &&
		   gBuildingPipeSwitch != PLATFORM_PS2 &&
		   gBuildingPipeSwitch != PLATFORM_PC)
			gBuildingPipeSwitch = PLATFORM_PS2;
	}else
		gBuildingPipeSwitch = PLATFORM_PS2;
}

static void
sanitizeAutomaticBackupSettings(void)
{
	if(gAutomaticBackupIntervalSeconds < 10)
		gAutomaticBackupIntervalSeconds = 10;
	if(gAutomaticBackupIntervalSeconds > 24*60*60)
		gAutomaticBackupIntervalSeconds = 24*60*60;
	if(gAutomaticBackupKeepCount < 1)
		gAutomaticBackupKeepCount = 1;
	if(gAutomaticBackupKeepCount > 100)
		gAutomaticBackupKeepCount = 100;
}

static bool
runAutomaticBackup(bool manual)
{
	char backupRoot[2048];
	FileLoader::AutomaticBackupResult result;
	uint32 latestSeq = GetLatestChangeSeq();

	sanitizeAutomaticBackupSettings();
	if(!GetArianeDataPath(backupRoot, sizeof(backupRoot), "automatic_backups")){
		if(manual)
			Toast(TOAST_SAVE, "Automatic Backup: failed to resolve backup folder");
		return false;
	}

	result = FileLoader::CreateAutomaticBackup(backupRoot, gAutomaticBackupKeepCount);
	gAutomaticBackupSecondsSinceLastRun = 0.0f;

	if(result.createdSnapshot){
		if(!result.hadWarnings)
			gAutomaticBackupLastHandledSeq = latestSeq;
		strncpy(gAutomaticBackupLastSnapshot, result.snapshotPath, sizeof(gAutomaticBackupLastSnapshot));
		gAutomaticBackupLastSnapshot[sizeof(gAutomaticBackupLastSnapshot)-1] = '\0';
		if(manual)
			Toast(TOAST_SAVE, "Automatic Backup: %d text + %d streamed file(s)",
			      result.numTextFiles, result.numBinaryFiles);
		else
			log("Automatic Backup: created %s (%d text, %d streamed, %d warning(s))\n",
			    result.snapshotPath, result.numTextFiles, result.numBinaryFiles, result.numErrors);
		if(!manual && result.hadWarnings)
			Toast(TOAST_SAVE, "Automatic Backup: created with %d warning(s)", result.numErrors);
		return true;
	}

	if(result.numErrors > 0){
		Toast(TOAST_SAVE, "Automatic Backup: failed with %d warning(s)", result.numErrors);
		return false;
	}

	gAutomaticBackupLastHandledSeq = latestSeq;
	if(manual)
		Toast(TOAST_SAVE, "Automatic Backup: nothing to back up");
	return false;
}

static void
automaticBackupTick(void)
{
	float dt = ImGui::GetIO().DeltaTime;
	uint32 latestSeq = GetLatestChangeSeq();

	if(latestSeq != gAutomaticBackupLastSeenSeq){
		gAutomaticBackupLastSeenSeq = latestSeq;
		gAutomaticBackupSecondsSinceLastChange = 0.0f;
	}else
		gAutomaticBackupSecondsSinceLastChange += dt;

	gAutomaticBackupSecondsSinceLastRun += dt;

	if(!gAutomaticBackupsEnabled)
		return;
	if(latestSeq == gAutomaticBackupLastHandledSeq)
		return;
	if(gAutomaticBackupSecondsSinceLastRun < (float)gAutomaticBackupIntervalSeconds)
		return;
	if(gAutomaticBackupSecondsSinceLastChange < gAutomaticBackupIdleSeconds)
		return;
	if(gGizmoUsing)
		return;

	runAutomaticBackup(false);
}

#ifdef _WIN32
static bool
findFileRecursive(const char *dir, const char *name)
{
	char searchPath[2048];
	if(!buildPath(searchPath, sizeof(searchPath), dir, "*"))
		return false;

	WIN32_FIND_DATAA entry;
	HANDLE handle = FindFirstFileA(searchPath, &entry);
	if(handle == INVALID_HANDLE_VALUE)
		return false;

	bool found = false;
	do{
		if(strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0)
			continue;

		if(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
			if(entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
				continue;

			char childDir[2048];
			if(buildPath(childDir, sizeof(childDir), dir, entry.cFileName) &&
			   findFileRecursive(childDir, name)){
				found = true;
				break;
			}
		}else if(_stricmp(entry.cFileName, name) == 0){
			found = true;
			break;
		}
	}while(FindNextFileA(handle, &entry));

	FindClose(handle);
	return found;
}
#endif

static const char*
getRequiredTeleportAsiName(void)
{
	if(isIII() || isVC() || isSA()) return "ariane.asi";
	return nil;
}

static bool
warnMissingTeleportAsi(const char *actionName)
{
	const char *asiName = getRequiredTeleportAsiName();
	if(asiName == nil)
		return true;

	char rootDir[2048];
	if(!getEditorRootDirectory(rootDir, sizeof(rootDir)))
		return true;

#ifdef _WIN32
	if(findFileRecursive(rootDir, asiName))
		return true;

	char message[1024];
	snprintf(message, sizeof(message),
		"%s will not work because %s was not found under:\n%s\n\n"
		"Install the Ariane plugin in the game folder (root or subfolder) and try again.",
		actionName, asiName, rootDir);
	MessageBoxA(nil, message, "Ariane", MB_OK | MB_ICONWARNING);
	Toast(TOAST_SAVE, "%s will not work: missing %s", actionName, asiName);
	return false;
#else
	return true;
#endif
}

static const char*
getCurrentGameExecutableName(void)
{
	if(isIII()) return "gta3.exe";
	if(isVC()) return "gta-vc.exe";
	if(isSA()) return "gta_sa.exe";
	return nil;
}

#ifdef _WIN32
static bool
isProcessRunningByName(const char *exeName)
{
	if(exeName == nil || exeName[0] == '\0')
		return false;

	DWORD processIds[2048];
	DWORD bytesReturned = 0;
	if(!EnumProcesses(processIds, sizeof(processIds), &bytesReturned))
		return false;

	const DWORD processCount = bytesReturned / sizeof(DWORD);
	for(DWORD i = 0; i < processCount; i++){
		const DWORD pid = processIds[i];
		if(pid == 0)
			continue;

		HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if(process == nil)
			continue;

		char imagePath[MAX_PATH];
		DWORD imagePathSize = sizeof(imagePath);
		bool match = false;
		if(QueryFullProcessImageNameA(process, 0, imagePath, &imagePathSize)){
			const char *baseName = strrchr(imagePath, '\\');
			if(baseName == nil)
				baseName = strrchr(imagePath, '/');
			baseName = baseName ? baseName + 1 : imagePath;
			match = _stricmp(baseName, exeName) == 0;
		}
		CloseHandle(process);

		if(match)
			return true;
	}
	return false;
}
#endif

static bool
isCurrentGameRunning(void)
{
#ifdef _WIN32
	const char *exeName = getCurrentGameExecutableName();
	return exeName && isProcessRunningByName(exeName);
#else
	return false;
#endif
}

// Toast notification system
#define TOAST_MAX 5
#define TOAST_DURATION 2.0f
#define TOAST_FADE_IN 0.15f
#define TOAST_FADE_OUT 0.4f

struct ToastEntry {
	char text[128];
	float timer;		// time remaining (counts down)
	float totalTime;	// total lifetime
	ToastCategory category;
};

static ToastEntry toasts[TOAST_MAX];
static int numToasts;
static bool toastEnabled = true;
static bool toastCategoryEnabled[TOAST_NUM_CATEGORIES] = { true, true, true, true, true, true };
static const char *toastCategoryNames[TOAST_NUM_CATEGORIES] = {
	"Undo / Redo", "Delete", "Copy / Paste", "Save", "Selection", "Spawn"
};

void
Toast(ToastCategory cat, const char *fmt, ...)
{
	if(!toastEnabled || !toastCategoryEnabled[cat])
		return;

	// Shift existing toasts down if full
	if(numToasts >= TOAST_MAX){
		memmove(&toasts[0], &toasts[1], (TOAST_MAX-1)*sizeof(ToastEntry));
		numToasts = TOAST_MAX - 1;
	}

	ToastEntry *t = &toasts[numToasts++];
	va_list args;
	va_start(args, fmt);
	vsnprintf(t->text, sizeof(t->text), fmt, args);
	va_end(args);
	t->totalTime = TOAST_DURATION + TOAST_FADE_IN + TOAST_FADE_OUT;
	t->timer = t->totalTime;
	t->category = cat;
}

static void
uiToasts(void)
{
	if(numToasts == 0) return;

	float dt = ImGui::GetIO().DeltaTime;
	float screenW = ImGui::GetIO().DisplaySize.x;
	float screenH = ImGui::GetIO().DisplaySize.y;

	// Update timers and remove expired
	for(int i = 0; i < numToasts; ){
		toasts[i].timer -= dt;
		if(toasts[i].timer <= 0.0f){
			memmove(&toasts[i], &toasts[i+1], (numToasts-i-1)*sizeof(ToastEntry));
			numToasts--;
		}else{
			i++;
		}
	}

	// Render from bottom up, centered horizontally
	float yBase = screenH - 60.0f;
	float spacing = 32.0f;

	for(int i = numToasts - 1; i >= 0; i--){
		ToastEntry *t = &toasts[i];
		float elapsed = t->totalTime - t->timer;

		// Compute alpha: fade in -> hold -> fade out
		float alpha;
		if(elapsed < TOAST_FADE_IN)
			alpha = elapsed / TOAST_FADE_IN;
		else if(t->timer < TOAST_FADE_OUT)
			alpha = t->timer / TOAST_FADE_OUT;
		else
			alpha = 1.0f;

		// Slide up slightly on appear
		float slideOffset = 0.0f;
		if(elapsed < TOAST_FADE_IN)
			slideOffset = (1.0f - elapsed / TOAST_FADE_IN) * 10.0f;

		ImVec2 textSize = ImGui::CalcTextSize(t->text);
		float padX = 16.0f, padY = 8.0f;
		float boxW = textSize.x + padX * 2;
		float boxH = textSize.y + padY * 2;
		float x = (screenW - boxW) * 0.5f;
		float y = yBase - (numToasts - 1 - i) * spacing + slideOffset;

		ImGui::SetNextWindowPos(ImVec2(x, y));
		ImGui::SetNextWindowSize(ImVec2(boxW, boxH));
		ImGui::SetNextWindowBgAlpha(0.0f);

		char winId[32];
		snprintf(winId, sizeof(winId), "##toast%d", i);
		ImGui::Begin(winId, nil,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBringToFrontOnFocus);

		ImDrawList *dl = ImGui::GetWindowDrawList();

		// Rounded rect background
		ImU32 bgCol = IM_COL32(20, 20, 20, (int)(200 * alpha));
		ImU32 borderCol = IM_COL32(80, 80, 80, (int)(150 * alpha));
		ImVec2 p0(x, y);
		ImVec2 p1(x + boxW, y + boxH);
		dl->AddRectFilled(p0, p1, bgCol, 6.0f);
		dl->AddRect(p0, p1, borderCol, 6.0f);

		// Text
		ImU32 textCol = IM_COL32(240, 240, 240, (int)(255 * alpha));
		dl->AddText(ImVec2(x + padX, y + padY), textCol, t->text);

		ImGui::End();
	}
}

static void
uiNotificationSettings(void)
{
	ImGui::Checkbox("Enable Notifications", &toastEnabled);
	if(toastEnabled){
		ImGui::Indent();
		for(int i = 0; i < TOAST_NUM_CATEGORIES; i++)
			ImGui::Checkbox(toastCategoryNames[i], &toastCategoryEnabled[i]);
ImGui::Unindent();
		}
	}
	if(Cars::HasCarSpawns()){
			ImGui::SeparatorText("Car Spawns");
			ImGui::Checkbox("Draw Car Spawns", &gRenderCarSpawns);
			if(gRenderCarSpawns){
				ImGui::Indent();
				ImGui::Checkbox("As Cubes", &Cars::gRenderAsCubes);
				ImGui::SameLine();
				ImGui::Checkbox("Angle", &Cars::gRenderAngle);
				ImGui::SeparatorText("Properties");
				ImGui::Checkbox("Vehicle ID", &Cars::gRenderVehicleId);
				ImGui::SameLine();
				ImGui::Checkbox("Primary Color", &Cars::gRenderPrimaryColor);
				ImGui::SameLine();
				ImGui::Checkbox("Secondary Color", &Cars::gRenderSecondaryColor);
				ImGui::Checkbox("Force Spawn", &Cars::gRenderForceSpawn);
				ImGui::SameLine();
				ImGui::Checkbox("Alarm Prob", &Cars::gRenderAlarmProb);
				ImGui::SameLine();
				ImGui::Checkbox("Locked Prob", &Cars::gRenderLockedProb);
				ImGui::Checkbox("Unknown 1", &Cars::gRenderUnknown1);
				ImGui::SameLine();
				ImGui::Checkbox("Unknown 2", &Cars::gRenderUnknown2);
				ImGui::Checkbox("File Name", &Cars::gRenderFileName);
				if(ImGui::Button("Export CSV")){
					Cars::ExportCSV();
				}
				ImGui::Unindent();
			}
		}
	}


	ImGui::Checkbox("Draw Water", &gRenderWater);
	if(params.water == GAME_SA){
		ImGui::SameLine();
		if(ImGui::Button("Edit Water (H)")){
			if(!WaterLevel::gWaterEditMode){
				WaterLevel::gWaterEditMode = true;
				ClearSelection();
				if(gPlaceMode)
					SpawnExitPlaceMode();
			}
		}
	}
	if(gameversion == GAME_SA)
		ImGui::Checkbox("Play Animations", &gPlayAnimations);

//<<<<<<< HEAD
	static int render = 0;
	//ImGui::RadioButton("Render Normal", &render, 0);
	//ImGui::RadioButton("Render only HD", &render, 1);
	//ImGui::RadioButton("Render only LOD", &render, 2);
	//gRenderOnlyHD = !!(render&1);
	//gRenderOnlyLod = !!(render&2);
	ImGui::Checkbox("Auto LOD Transition", &gAutoAnimateLOD);
//=======
	ImGui::RadioButton("Render Normal", &gRenderMode, 0);
	ImGui::RadioButton("Render only HD", &gRenderMode, 1);
	ImGui::RadioButton("Render only LOD", &gRenderMode, 2);
	gRenderOnlyHD = gRenderMode == 1;
	gRenderOnlyLod = gRenderMode == 2;
//>>>>>>> remotes/upstream/master
	ImGui::SliderFloat("Draw Distance", &TheCamera.m_LODmult, 0.5f, 3.0f, "%.3f");
	ImGui::Checkbox("Render all Timed Objects", &gNoTimeCull);
	if(params.numAreas)
		ImGui::Checkbox("Render all Areas", &gNoAreaCull);

	ImGui::Separator();
	ImGui::Text("IPL Visibility");
	RefreshIplVisibilityEntries();

	int numIpls = GetIplVisibilityEntryCount();
	if(numIpls == 0){
		ImGui::TextDisabled("No loaded IPLs");
		return;
	}

	int numVisible = 0;
	for(int i = 0; i < numIpls; i++)
		if(GetIplVisibilityEntryVisible(i))
			numVisible++;
	ImGui::Text("%d visible / %d total", numVisible, numIpls);
	ImGui::InputTextWithHint("##ipl_filter_search", "Search IPLs", gIplFilterSearch, sizeof(gIplFilterSearch));
	if(ImGui::Button("Show All"))
		SetAllIplVisibilityEntries(true);
	ImGui::SameLine();
	if(ImGui::Button("Hide All"))
		SetAllIplVisibilityEntries(false);

	float listHeight = ImGui::GetContentRegionAvail().y;
	if(listHeight < 220.0f)
		listHeight = 220.0f;
	ImGui::BeginChild("##ipl_visibility_list", ImVec2(0, listHeight), true);
	for(int i = 0; i < numIpls; i++){
		const char *name = GetIplVisibilityEntryName(i);
		if(gIplFilterSearch[0] != '\0' && ImStristr(name, nil, gIplFilterSearch, nil) == nil)
			continue;

		bool visible = GetIplVisibilityEntryVisible(i);
		ImGui::PushID(i);
		if(ImGui::SmallButton("Only"))
			ShowOnlyIplVisibilityEntry(i);
		ImGui::SameLine();
		if(ImGui::Checkbox("##visible", &visible))
			SetIplVisibilityEntryVisible(i, visible);
		ImGui::SameLine();
		ImGui::TextUnformatted(name);
		ImGui::PopID();
	}
	ImGui::EndChild();
}

static void
uiRendering(void)
{
	ImGui::Checkbox("Draw PostFX", &gRenderPostFX);
	if(params.timecycle == GAME_VC){
		ImGui::Checkbox("Use Blur Ambient", &gUseBlurAmb); ImGui::SameLine();
		ImGui::Checkbox("Override", &gOverrideBlurAmb);
	}
	if(params.timecycle == GAME_SA){
		ImGui::Text("Colour filter"); ImGui::SameLine();
		ImGui::RadioButton("None##NOPOSTFX", &gColourFilter, 0); ImGui::SameLine();
		ImGui::RadioButton("PS2##PS2POSTFX", &gColourFilter, PLATFORM_PS2); ImGui::SameLine();
		ImGui::RadioButton("PC/Xbox##PCPOSTFX", &gColourFilter, PLATFORM_PC); ImGui::SameLine();
		ImGui::Checkbox("Radiosity", &gRadiosity);
	}
	if(params.timecycle == GAME_LCS || params.timecycle == GAME_VCS){
		ImGui::Text("Colour filter"); ImGui::SameLine();
		ImGui::RadioButton("None##NOPOSTFX", &gColourFilter, 0); ImGui::SameLine();
		ImGui::RadioButton("PS2##PS2POSTFX", &gColourFilter, PLATFORM_PS2); ImGui::SameLine();
		ImGui::RadioButton("PSP##PCPOSTFX", &gColourFilter, PLATFORM_PSP);
		if(params.timecycle == GAME_VCS){
			 ImGui::SameLine();
			ImGui::Checkbox("Radiosity", &gRadiosity);
		}
	}
	if(params.daynightPipe){
		ImGui::Text("Building Pipe"); ImGui::SameLine();
		ImGui::RadioButton("PS2##PS2BUILD", &gBuildingPipeSwitch, PLATFORM_PS2); ImGui::SameLine();
		ImGui::RadioButton("PC##PCBUILD", &gBuildingPipeSwitch, PLATFORM_PC); ImGui::SameLine();
		ImGui::RadioButton("Xbox##XBOXBUILD", &gBuildingPipeSwitch, PLATFORM_XBOX);
	}
	if(params.leedsPipe){
		ImGui::Text("Building Pipe"); ImGui::SameLine();
		ImGui::RadioButton("Default##NONE", &gBuildingPipeSwitch, PLATFORM_NULL); ImGui::SameLine();
		ImGui::RadioButton("PSP##PSPBUILD", &gBuildingPipeSwitch, PLATFORM_PSP); ImGui::SameLine();
		ImGui::RadioButton("PS2##PS2BUILD", &gBuildingPipeSwitch, PLATFORM_PS2); ImGui::SameLine();
		ImGui::RadioButton("Mobile##MOBILEBUILD", &gBuildingPipeSwitch, PLATFORM_PC);
	}
	ImGui::Checkbox("Backface Culling", &gDoBackfaceCulling);
	// TODO: not params
	ImGui::Checkbox("PS2 Alpha test", &params.ps2AlphaTest);
	ImGui::InputInt("Alpha Ref", &params.alphaRef, 1);
	if(params.alphaRef < 0) params.alphaRef = 0;
	if(params.alphaRef > 255) params.alphaRef = 255;

	ImGui::Checkbox("Draw Background", &gRenderBackground);
	ImGui::Checkbox("Enable Fog", &gEnableFog);
	if(params.timecycle == GAME_SA)
		ImGui::Checkbox("Enable TimeCycle boxes", &gEnableTimecycleBoxes);
}

static void
uiFilteredInstanceList(ObjectDef *obj)
{
	static char buf[256];
	CPtrNode *p;
	ObjectInst *inst;
	for(p = instances.first; p; p = p->next){
		inst = (ObjectInst*)p->item;
		if(GetObjectDef(inst->m_objectId) != obj)
			continue;
		bool pop = false;
		if(inst->m_selected){
			ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(255, 0, 0));
			pop = true;
		}
		ImGui::PushID(inst);
		sprintf(buf, "%-20s %8.2f %8.2f %8.2f", obj->m_name,
			inst->m_translation.x, inst->m_translation.y, inst->m_translation.z);
		ImGui::Selectable(buf);
		ImGui::PopID();
		if(ImGui::IsItemHovered()){
			if(ImGui::IsMouseClicked(1))
				inst->Select();
			if(ImGui::IsMouseDoubleClicked(0))
				inst->JumpTo();
		}
		if(pop)
			ImGui::PopStyleColor();
		if(ImGui::IsItemHovered())
			inst->m_highlight = HIGHLIGHT_HOVER;
	}
}

int uiNumCarPathColumns(void) { return isIII() ? 9 : isSA() ? 14 : 13; }

void
uiCarPathHeader(void)
{
	ImGui::TableSetupColumn("idx");
	ImGui::TableSetupColumn("type");
	ImGui::TableSetupColumn("link");
	ImGui::TableSetupColumn("numLinks");
	ImGui::TableSetupColumn("x");
	ImGui::TableSetupColumn("y");
	ImGui::TableSetupColumn("z");
	ImGui::TableSetupColumn("lanesIn");
	ImGui::TableSetupColumn("lanesOut");
	if(!isIII()){
		ImGui::TableSetupColumn("width");
		ImGui::TableSetupColumn("speed");
		ImGui::TableSetupColumn("flags");
		ImGui::TableSetupColumn("density");
		if(isSA())
			ImGui::TableSetupColumn("special");
	}
	ImGui::TableHeadersRow();
}

void
uiCarPathNode(PathNode *nd, int i, ObjectInst *inst)
{
	int c = 0;
	ImGui::TableSetColumnIndex(c++);
	char str[50];
	sprintf(str, "%d", i);
	if(ImGui::Selectable(str, nd == Path::selectedNode, ImGuiSelectableFlags_SpanAllColumns))
		Path::selectedNode = nd;
	if(ImGui::IsItemHovered()){
		Path::guiHoveredNode = nd;
		if(ImGui::IsMouseDoubleClicked(0))
			nd->JumpTo(inst);
	}
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text(nd->type == PathNode::NodeInternal ? "intern" : "extern");
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text("%d", nd->link);
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text("%d", nd->numLinks);
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text("%g", nd->x*16);
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text("%g", nd->y*16);
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text("%g", nd->z*16);
	if(nd->type == PathNode::NodeExternal){
		ImGui::TableSetColumnIndex(c++);
		ImGui::Text("%d", nd->lanesIn);
		ImGui::TableSetColumnIndex(c++);
		ImGui::Text("%d", nd->lanesOut);
	}else{
		ImGui::TableSetColumnIndex(c++);
		ImGui::Text(" ");
		ImGui::TableSetColumnIndex(c++);
		ImGui::Text(" ");
	}

	if(!isIII()){
		ImGui::TableSetColumnIndex(c++);
		ImGui::Text("%g", nd->width);
		ImGui::TableSetColumnIndex(c++);
		ImGui::Text("%d", nd->speed);
		ImGui::TableSetColumnIndex(c++);
		ImGui::Text("%X", nd->flags);
		ImGui::TableSetColumnIndex(c++);
		ImGui::Text("%g", nd->density);
		if(isSA()){
			ImGui::TableSetColumnIndex(c++);
			ImGui::Text("%d", nd->special);
		}
	}
}

void
uiPedPathHeader(void)
{
	ImGui::TableSetupColumn("idx");
	ImGui::TableSetupColumn("type");
	ImGui::TableSetupColumn("link");
	ImGui::TableSetupColumn("cross");
	ImGui::TableSetupColumn("numLinks");
	ImGui::TableSetupColumn("x");
	ImGui::TableSetupColumn("y");
	ImGui::TableSetupColumn("z");
	ImGui::TableHeadersRow();
}

void
uiPedPathNode(PathNode *nd, int i, ObjectInst *inst)
{
	int c = 0;
	ImGui::TableSetColumnIndex(c++);
	char str[50];
	sprintf(str, "%d", i);
	if(ImGui::Selectable(str, nd == Path::selectedNode, ImGuiSelectableFlags_SpanAllColumns))
		Path::selectedNode = nd;
	if(ImGui::IsItemHovered()){
		Path::guiHoveredNode = nd;
		if(ImGui::IsMouseDoubleClicked(0))
			nd->JumpTo(inst);
	}
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text(nd->type == PathNode::NodeInternal ? "intern" : "extern");
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text("%d", nd->link);
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text("%d", nd->linkType);
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text("%d", nd->numLinks);
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text("%g", nd->x*16);
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text("%g", nd->y*16);
	ImGui::TableSetColumnIndex(c++);
	ImGui::Text("%g", nd->z*16);
}

static void
uiPathInfo(ObjectInst *inst)
{
	if(inst){
		ObjectDef *obj;
		obj = GetObjectDef(inst->m_objectId);

		ImGui::TextDisabled("Legacy object-attached path patches");

		if(obj->m_carPathIndex >= 0){
			PathNode *nd = Path::GetCarNode(obj->m_carPathIndex,0);
			ImGui::Text(nd->water ? "Legacy Water Path" : "Legacy Car Path");
			if(ImGui::BeginTable("Nodes", uiNumCarPathColumns(), ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)){
				uiCarPathHeader();
				for(int i = 0; nd = Path::GetCarNode(obj->m_carPathIndex,i); i++){
					ImGui::TableNextRow();
					uiCarPathNode(nd, i, inst);
				}
				ImGui::EndTable();
			}
		}
		if(obj->m_pedPathIndex >= 0){
			ImGui::Text("Legacy Ped Path");
			PathNode *nd;
			if(ImGui::BeginTable("Nodes", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)){
				uiPedPathHeader();
				for(int i = 0; nd = Path::GetPedNode(obj->m_pedPathIndex,i); i++){
					ImGui::TableNextRow();
					uiPedPathNode(nd, i, inst);
				}
				ImGui::EndTable();
			}
		}
	}else if(Path::selectedNode && !Path::selectedNode->isDetached()){
		ObjectDef *obj = GetObjectDef(Path::selectedNode->objId);
		ImGui::Text("Object %s", obj->m_name);
		ImGui::TextDisabled("Legacy object-attached path patch");
		uiFilteredInstanceList(obj);
	}else if(Path::selectedNode && Path::selectedNode->tabId == 1){
		int i = Path::selectedNode->idx;
		ImGui::Text(Path::selectedNode->water ? "Legacy Water Path %d" : "Legacy Car Path %d", i);
		ImGui::PushID(i);
		if(ImGui::BeginTable("Nodes", uiNumCarPathColumns(), ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)){
			uiCarPathHeader();
			for(int j = 0; j < 12; j++){
				PathNode *nd = Path::GetDetachedCarNode(i,j);
				if(nd == nil) break;
				ImGui::TableNextRow();
				uiCarPathNode(nd, j, nil);
			}
			ImGui::EndTable();
		}
		ImGui::PopID();
	}else if(Path::selectedNode && Path::selectedNode->tabId == 3){
		int i = Path::selectedNode->idx;
		ImGui::Text("Legacy Ped Path %d", i);
		ImGui::PushID(i);
		if(ImGui::BeginTable("Nodes", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)){
			uiPedPathHeader();
			for(int j = 0; j < 12; j++){
				PathNode *nd = Path::GetDetachedPedNode(i,j);
				if(nd == nil) break;
				ImGui::TableNextRow();
				uiPedPathNode(nd, j, nil);
			}
			ImGui::EndTable();
		}
		ImGui::PopID();
	}
}

static const char *fxTypeNames[] = {
	"Light",
	"Particle",
	"LookAtPoint",
	"PedQueue",
	"SunGlare",
	"Interior",
	"EntryExit",
	"Roadsign",
	"TriggerPoint",
	"CoverPoint",
	"Escalator"
};
static const char *flareTypeNames[] = { "None", "Sun", "Headlight" };

namespace Effects {
const char*
GetEffectTypeName(int type)
{
	if(type < 0 || type >= (int)IM_ARRAYSIZE(fxTypeNames))
		return "Unknown";
	return fxTypeNames[type];
}
}

void
uiOneEffect(Effect *e)
{
	ImGui::Combo("Effect Type", &e->type, fxTypeNames, IM_ARRAYSIZE(fxTypeNames));
	ImGui::DragFloat3("Position", &e->pos.x, 0.1f);

	rw::RGBAf col;
	convColor(&col, &e->col);
	if(ImGui::ColorEdit4("Color", (float*)&col))
		convColor(&e->col, &col);

	ImGui::Separator();

	switch(e->type){
	case FX_LIGHT: {
		ImGui::DragFloat("LOD dist",     &e->light.lodDist,    1.f);
		ImGui::DragFloat("Size",         &e->light.size,       0.01f);
		ImGui::DragFloat("Corona size",  &e->light.coronaSize, 0.01f);
		ImGui::DragFloat("Shadow size",  &e->light.shadowSize, 0.01f);
		ImGui::Separator();
		ImGui::DragInt("Flashiness",     &e->light.flashiness);
		ImGui::DragInt("Shadow alpha",   &e->light.shadowAlpha, 1, 0, 255);

		ImGui::Combo("Lens flare", &e->light.lensFlareType, flareTypeNames, IM_ARRAYSIZE(flareTypeNames));

		bool refl = !!e->light.reflection;
		if(ImGui::Checkbox("Reflection", &refl))
			e->light.reflection = !!refl;

		ImGui::InputInt("Flags", &e->light.flags, 1, 1, ImGuiInputTextFlags_CharsHexadecimal);
		ImGui::Separator();
		ImGui::InputText("Corona tex", e->light.coronaTex, 32);
		ImGui::InputText("Shadow tex", e->light.shadowTex, 32);
		} break;

	case FX_PARTICLE:
		ImGui::DragInt   ("Particle type", &e->prtcl.particleType);
		ImGui::DragFloat3("Direction",     &e->prtcl.dir.x, 0.01f);
		ImGui::DragFloat ("Size",          &e->prtcl.size,  0.01f);
		break;

	case FX_LOOKATPOINT:
		ImGui::DragFloat3("Direction",   &e->look.dir.x, 0.01f);
		ImGui::DragInt   ("Type",        &e->look.type);
		ImGui::DragInt   ("Probability", &e->look.probability, 1, 0, 100);
		break;

	case FX_PEDQUEUE:
		ImGui::DragFloat3("Queue dir", &e->queue.queueDir.x, 0.01f);
		ImGui::DragFloat3("Use dir",   &e->queue.useDir.x,   0.01f);
		ImGui::DragFloat3("Forward dir", &e->queue.forwardDir.x, 0.01f);
		ImGui::DragInt   ("Type",      &e->queue.type);
		break;

	case FX_INTERIOR:
		ImGui::TextDisabled("Render-only preview");
		ImGui::Text("Type: %d", e->interior.type);
		ImGui::Text("Group: %d", e->interior.group);
		ImGui::Text("Size: %.1f x %.1f x %.1f", e->interior.width, e->interior.depth, e->interior.height);
		break;

	case FX_ENTRYEXIT:
		ImGui::TextDisabled("Render-only preview");
		ImGui::Text("Area: %d", e->entryExit.areaCode);
		ImGui::Text("Radius: %.2f x %.2f", e->entryExit.radiusX, e->entryExit.radiusY);
		ImGui::Text("Title: %.8s", e->entryExit.title);
		break;

	case FX_ROADSIGN:
		ImGui::TextDisabled("Render-only preview");
		ImGui::Text("Size: %.2f x %.2f", e->roadsign.width, e->roadsign.height);
		ImGui::Text("Line 1: %.16s", e->roadsign.text[0]);
		ImGui::Text("Line 2: %.16s", e->roadsign.text[1]);
		ImGui::Text("Line 3: %.16s", e->roadsign.text[2]);
		ImGui::Text("Line 4: %.16s", e->roadsign.text[3]);
		break;

	case FX_TRIGGERPOINT:
		ImGui::TextDisabled("Render-only preview");
		ImGui::Text("Index: %d", e->triggerPoint.index);
		break;

	case FX_COVERPOINT:
		ImGui::TextDisabled("Render-only preview");
		ImGui::Text("Direction: %.2f %.2f", e->coverPoint.dirX, e->coverPoint.dirY);
		ImGui::Text("Usage: %d", e->coverPoint.usage);
		break;

	case FX_ESCALATOR:
		ImGui::TextDisabled("Render-only preview");
		ImGui::Text("Direction: %s", e->escalator.goingUp ? "Up" : "Down");
		break;
	}
}

static void
uiFxTable(ObjectInst *inst)
{
	if(inst == nil)
		return;

	ObjectDef *obj;
	obj = GetObjectDef(inst->m_objectId);

	ImGui::Text("Effects (%d)", obj->m_numEffects);
	ImGui::Separator();

	ImGui::BeginChild("##effect_list", ImVec2(0, 0), false);

	for(int i = 0; i < obj->m_numEffects; i++) {
		Effect *e = Effects::GetEffect(obj->m_effectIndex+i);
		ImGui::ColorButton("##col", mkColor(e->col),
				ImGuiColorEditFlags_NoTooltip |
				ImGuiColorEditFlags_NoBorder,
				ImVec2(12, 12));
		ImGui::SameLine();

		ImGui::TextDisabled("%2d", i);
		ImGui::SameLine();

		char label[64];
		snprintf(label, sizeof(label), "%s##eff%d", Effects::GetEffectTypeName(e->type), i);

		if(ImGui::Selectable(label, e == Effects::selectedEffect, ImGuiSelectableFlags_None, ImVec2(0, 0)))
			Effects::selectedEffect = e;
		if(ImGui::IsItemHovered()){
			Effects::guiHoveredEffect = e;
			if(ImGui::IsMouseClicked(1))
				Effects::selectedEffect = e;
			if(ImGui::IsMouseDoubleClicked(0))
				e->JumpTo(inst);
		}
	}
	ImGui::EndChild();
}

static void
uiFxInfo(ObjectInst *inst)
{
	float listWidth = 200.f;
	ImGui::BeginChild("##left", ImVec2(listWidth, 0), true);
	uiFxTable(inst);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("##right", ImVec2(0, 0), true);
	if(Effects::selectedEffect)
		uiOneEffect(Effects::selectedEffect);
	else
		ImGui::TextDisabled("Select an effect");
	ImGui::EndChild();
}


static void
uiInstInfo(ObjectInst *inst)
{
	ObjectDef *obj;
	obj = GetObjectDef(inst->m_objectId);

	InputTextReadonly<MODELNAMELEN>("Model##Inst", obj->m_name);
	InputTextReadonly<1024>("IPL", inst->m_file->name);

	if(inst->m_isDeleted){
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(255, 80, 80));
		ImGui::Text("DELETED");
		ImGui::PopStyleColor();
		ImGui::SameLine();
		if(ImGui::Button("Undelete"))
			inst->Undelete();
	}else{
		if(ImGui::Button("Delete")){
			inst->Delete();
		}
		if(inst->m_lod){
			ImGui::SameLine();
			ImGui::TextDisabled("(LOD: %s)", GetObjectDef(inst->m_lod->m_objectId)->m_name);
		}
	}

	bool changed = false;
	changed |= ImGui::DragFloat3("Translation", (float*)&inst->m_translation, 0.1f);
	ImGui::Text("Rotation: %.3f %.3f %.3f %.3f",
		inst->m_rotation.x,
		inst->m_rotation.y,
		inst->m_rotation.z,
		inst->m_rotation.w);
	if(changed){
		StampChangeSeq(inst);
		inst->m_isDirty = true;
		inst->UpdateMatrix();
		if(inst->m_rwObject){
			rw::Frame *f;
			if(obj->m_type == ObjectDef::ATOMIC)
				f = ((rw::Atomic*)inst->m_rwObject)->getFrame();
			else
				f = ((rw::Clump*)inst->m_rwObject)->getFrame();
			f->transform(&inst->m_matrix, rw::COMBINEREPLACE);
		}
	}

	ImGui::InputInt("Interior", &inst->m_area);
	if(inst->m_area < 0) inst->m_area = 0;

	if(params.objFlagset == GAME_SA){
		ImGui::Checkbox("Unimportant", &inst->m_isUnimportant);
		ImGui::Checkbox("Underwater", &inst->m_isUnderWater);
		ImGui::Checkbox("Tunnel", &inst->m_isTunnel);
		ImGui::Checkbox("Tunnel Transition", &inst->m_isTunnelTransition);
	}
}

static void
uiObjInfo(ObjectDef *obj)
{
	int i;
	TxdDef *txd;

	txd = GetTxdDef(obj->m_txdSlot);

	ImGui::Text("ID: %d\n", obj->m_id);
	InputTextReadonly<MODELNAMELEN>("Model", obj->m_name);
	InputTextReadonly<MODELNAMELEN>("TXD", txd ? txd->name : "");

	InputTextReadonly<1024>("IDE", obj->m_file ? obj->m_file->name : "");
	if(obj->m_colModel && !obj->m_gotChildCol)
		InputTextReadonly<1024>("COL", obj->m_colModel->file ? obj->m_colModel->file->name : "");

	ImGui::Text("Draw dist:");
	for(i = 0; i < obj->m_numAtomics; i++){
		ImGui::SameLine();
		ImGui::Text("%.0f", obj->m_drawDist[i]);
	}
	ImGui::Text("Min Draw dist: %.0f", obj->m_minDrawDist);

	if(obj->m_isTimed){
		ImGui::Text("Time: %d %d (visible now: %s)",
			obj->m_timeOn, obj->m_timeOff,
			IsHourInRange(obj->m_timeOn, obj->m_timeOff) ? "yes" : "no");
	}

	if(obj->m_relatedModel)
		ImGui::Text("Related: %s\n", obj->m_relatedModel->m_name);
	if(obj->m_relatedTimeModel)
		ImGui::Text("Related timed: %s\n", obj->m_relatedTimeModel->m_name);

	uiObjectFlagsEditor(obj);

}

struct CamSetting {
	char name[256];
	rw::V3d pos;
	rw::V3d target;
	float fov;

	int hour, minute;
	int weather1, weather2;
	int extracolors;

	int area;
};

std::vector<CamSetting> camSettings;

static void
loadCamSettings(void)
{
	CamSetting cam;
	char line[256], *p, *pp;
	FILE *f;

	f = fopenArianeDataRead("camsettings.txt", "camsettings.txt");
	if(f == nil)
		return;
	camSettings.clear();
	while(fgets(line, sizeof(line), f)){
		p = line;
		while(*p && isspace(*p)) p++;
		if(*p != '"')
			continue;
		pp = ++p;
		while(*p && *p != '"') p++;
		if(*p != '"')
			continue;
		*p++ = '\0';
		strncpy(cam.name, pp, sizeof(cam.name));
		sscanf(p, "%f %f %f  %f %f %f  %f  %d %d %d %d  %d",
			&cam.pos.x, &cam.pos.y, &cam.pos.z,
			&cam.target.x, &cam.target.y, &cam.target.z,
			&cam.fov,
			&cam.hour, &cam.minute, &cam.weather1, &cam.weather2,
			&cam.area);
		if(cam.fov < 1.0f || cam.fov > 150.0f)
			cam.fov = 70.0f;
		if(cam.area < 0)
			cam.area = 0;
		cam.hour %= 24;
		cam.minute %= 60;
		cam.weather1 %= params.numWeathers;
		cam.weather2 %= params.numWeathers;
		camSettings.push_back(cam);
	}

	fclose(f);
}

static void
loadSaveSettings(void)
{
	FILE *f;
	char line[1024];
	char key[128];
	const char *value;
	int intValue;
	bool boolValue;
	rw::V3d vecValue;
	int savedSpawnObjectId = -1;

	sanitizeAutomaticBackupSettings();
	sanitizeCustomImportSettings();
	gAutomaticBackupLastSeenSeq = GetLatestChangeSeq();
	gAutomaticBackupLastHandledSeq = gAutomaticBackupLastSeenSeq;
	gAutomaticBackupLastSnapshot[0] = '\0';
	gRenderMode = gRenderOnlyHD ? 1 : gRenderOnlyLod ? 2 : 0;
	gSavedIplVisibilityStates.clear();
	gBrowserTabRestorePending = true;
	loadWindowStateFromSettingsFile();
	gPersistentSettingsLoaded = true;

	f = fopenArianeDataRead("savesettings.txt", "savesettings.txt");
	if(f == nil)
		return;

	while(fgets(line, sizeof(line), f)){
		if(!splitSettingLine(line, key, sizeof(key), &value))
			continue;
		if(strcmp(key, "save_destination") == 0){
			if(parseIntSetting(value, &intValue) && intValue == SAVE_DESTINATION_MODLOADER)
				gSaveDestination = SAVE_DESTINATION_MODLOADER;
			else
				gSaveDestination = SAVE_DESTINATION_ORIGINAL_FILES;
		}else if(strcmp(key, "automatic_backups") == 0){
			if(parseBoolSetting(value, &boolValue))
				gAutomaticBackupsEnabled = boolValue;
		}else if(strcmp(key, "automatic_backup_interval") == 0){
			parseIntSetting(value, &gAutomaticBackupIntervalSeconds);
		}else if(strcmp(key, "automatic_backup_keep") == 0){
			parseIntSetting(value, &gAutomaticBackupKeepCount);
		}else if(strcmp(key, "custom_import_start_id") == 0){
			parseIntSetting(value, &gCustomImportPreferredStartId);
		}else if(strcmp(key, "show_demo_window") == 0){
			if(parseBoolSetting(value, &boolValue)) showDemoWindow = boolValue;
		}else if(strcmp(key, "show_editor_window") == 0){
			if(parseBoolSetting(value, &boolValue)) showEditorWindow = boolValue;
		}else if(strcmp(key, "show_instance_window") == 0){
			if(parseBoolSetting(value, &boolValue)) showInstanceWindow = boolValue;
		}else if(strcmp(key, "show_log_window") == 0){
			if(parseBoolSetting(value, &boolValue)) showLogWindow = boolValue;
		}else if(strcmp(key, "show_help_window") == 0){
			if(parseBoolSetting(value, &boolValue)) showHelpWindow = boolValue;
		}else if(strcmp(key, "show_time_weather_window") == 0){
			if(parseBoolSetting(value, &boolValue)) showTimeWeatherWindow = boolValue;
		}else if(strcmp(key, "show_view_window") == 0){
			if(parseBoolSetting(value, &boolValue)) showViewWindow = boolValue;
		}else if(strcmp(key, "show_rendering_window") == 0){
			if(parseBoolSetting(value, &boolValue)) showRenderingWindow = boolValue;
		}else if(strcmp(key, "show_browser_window") == 0){
			if(parseBoolSetting(value, &boolValue)) showBrowserWindow = boolValue;
		}else if(strcmp(key, "show_diff_window") == 0){
			if(parseBoolSetting(value, &boolValue)) showDiffWindow = boolValue;
		}else if(strcmp(key, "show_tools_window") == 0){
			if(parseBoolSetting(value, &boolValue)) showToolsWindow = boolValue;
		}else if(strcmp(key, "toast_enabled") == 0){
			if(parseBoolSetting(value, &boolValue)) toastEnabled = boolValue;
		}else if(strncmp(key, "toast_category_", 15) == 0){
			int idx = atoi(key + 15);
			if(idx >= 0 && idx < TOAST_NUM_CATEGORIES && parseBoolSetting(value, &boolValue))
				toastCategoryEnabled[idx] = boolValue;
		}else if(strcmp(key, "time_hour") == 0){
			parseIntSetting(value, &currentHour);
		}else if(strcmp(key, "time_minute") == 0){
			parseIntSetting(value, &currentMinute);
		}else if(strcmp(key, "current_area") == 0){
			parseIntSetting(value, &currentArea);
		}else if(strcmp(key, "weather_old") == 0){
			parseIntSetting(value, &Weather::oldWeather);
		}else if(strcmp(key, "weather_new") == 0){
			parseIntSetting(value, &Weather::newWeather);
		}else if(strcmp(key, "weather_interpolation") == 0){
			parseFloatSetting(value, &Weather::interpolation);
		}else if(strcmp(key, "extra_colours") == 0){
			parseIntSetting(value, &extraColours);
		}else if(strcmp(key, "day_night_balance") == 0){
			parseFloatSetting(value, &gDayNightBalance);
		}else if(strcmp(key, "wet_road_effect") == 0){
			parseFloatSetting(value, &gWetRoadEffect);
		}else if(strcmp(key, "neo_light_map_strength") == 0){
			parseFloatSetting(value, &gNeoLightMapStrength);
		}else if(strcmp(key, "camera_position") == 0){
			if(parseVec3Setting(value, &vecValue))
				TheCamera.m_position = vecValue;
		}else if(strcmp(key, "camera_target") == 0){
			if(parseVec3Setting(value, &vecValue))
				TheCamera.m_target = vecValue;
		}else if(strcmp(key, "camera_fov") == 0){
			parseFloatSetting(value, &TheCamera.m_fov);
		}else if(strcmp(key, "draw_target") == 0){
			if(parseBoolSetting(value, &boolValue)) gDrawTarget = boolValue;
		}else if(strcmp(key, "render_collision") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderCollision = boolValue;
		}else if(strcmp(key, "render_zones") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderZones = boolValue;
		}else if(strcmp(key, "render_map_zones") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderMapZones = boolValue;
		}else if(strcmp(key, "render_navig_zones") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderNavigZones = boolValue;
		}else if(strcmp(key, "render_info_zones") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderInfoZones = boolValue;
		}else if(strcmp(key, "render_cull_zones") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderCullZones = boolValue;
		}else if(strcmp(key, "render_attrib_zones") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderAttribZones = boolValue;
		}else if(strcmp(key, "render_light_effects") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderLightEffects = boolValue;
		}else if(strcmp(key, "render_effect_markers") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderEffects = boolValue;
		}else if(strcmp(key, "render_legacy_ped_paths") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderLegacyPedPaths = boolValue;
		}else if(strcmp(key, "render_legacy_car_paths") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderLegacyCarPaths = boolValue;
		}else if(strcmp(key, "render_sa_ped_paths") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderSaPedPaths = boolValue;
		}else if(strcmp(key, "render_sa_ped_path_walkers") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderSaPedPathWalkers = boolValue;
		}else if(strcmp(key, "render_sa_car_paths") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderSaCarPaths = boolValue;
		}else if(strcmp(key, "render_sa_car_path_traffic") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderSaCarPathTraffic = boolValue;
		}else if(strcmp(key, "render_sa_area_grid") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderSaAreaGrid = boolValue;
		}else if(strcmp(key, "sa_ped_path_walker_count") == 0){
			parseIntSetting(value, &gSaPedPathWalkerCount);
		}else if(strcmp(key, "sa_car_path_traffic_count") == 0){
			parseIntSetting(value, &gSaCarPathTrafficCount);
		}else if(strcmp(key, "sa_car_path_traffic_speed_scale") == 0){
			parseFloatSetting(value, &gSaCarPathTrafficSpeedScale);
		}else if(strcmp(key, "sa_car_path_traffic_freeze_routes") == 0){
			if(parseBoolSetting(value, &boolValue)) gSaCarPathTrafficFreezeRoutes = boolValue;
		}else if(strcmp(key, "render_sa_car_path_parked_cars") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderSaCarPathParkedCars = boolValue;
		}else if(strcmp(key, "sa_car_path_parked_car_count") == 0){
			parseIntSetting(value, &gSaCarPathParkedCarCount);
		}else if(strcmp(key, "render_water") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderWater = boolValue;
		}else if(strcmp(key, "play_animations") == 0){
			if(parseBoolSetting(value, &boolValue)) gPlayAnimations = boolValue;
		}else if(strcmp(key, "render_mode") == 0){
			parseIntSetting(value, &gRenderMode);
		}else if(strcmp(key, "draw_distance") == 0){
			parseFloatSetting(value, &TheCamera.m_LODmult);
		}else if(strcmp(key, "render_all_timed_objects") == 0){
			if(parseBoolSetting(value, &boolValue)) gNoTimeCull = boolValue;
		}else if(strcmp(key, "render_all_areas") == 0){
			if(parseBoolSetting(value, &boolValue)) gNoAreaCull = boolValue;
		}else if(strcmp(key, "ipl_filter_search") == 0){
			parseQuotedStringValue(value, gIplFilterSearch, sizeof(gIplFilterSearch));
		}else if(strcmp(key, "ipl_visible") == 0){
			SavedIplVisibilityState state;
			const char *after = nil;
			memset(&state, 0, sizeof(state));
			if(parseQuotedStringValue(value, state.key, sizeof(state.key), &after) &&
			   parseBoolSetting(after, &state.visible))
				gSavedIplVisibilityStates.push_back(state);
		}else if(strcmp(key, "render_postfx") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderPostFX = boolValue;
		}else if(strcmp(key, "use_blur_ambient") == 0){
			if(parseBoolSetting(value, &boolValue)) gUseBlurAmb = boolValue;
		}else if(strcmp(key, "override_blur_ambient") == 0){
			if(parseBoolSetting(value, &boolValue)) gOverrideBlurAmb = boolValue;
		}else if(strcmp(key, "colour_filter") == 0){
			parseIntSetting(value, &gColourFilter);
		}else if(strcmp(key, "radiosity") == 0){
			if(parseBoolSetting(value, &boolValue)) gRadiosity = boolValue;
		}else if(strcmp(key, "building_pipe") == 0){
			parseIntSetting(value, &gBuildingPipeSwitch);
		}else if(strcmp(key, "backface_culling") == 0){
			if(parseBoolSetting(value, &boolValue)) gDoBackfaceCulling = boolValue;
		}else if(strcmp(key, "ps2_alpha_test") == 0){
			if(parseBoolSetting(value, &boolValue)) params.ps2AlphaTest = boolValue;
		}else if(strcmp(key, "alpha_ref") == 0){
			parseIntSetting(value, &params.alphaRef);
		}else if(strcmp(key, "render_background") == 0){
			if(parseBoolSetting(value, &boolValue)) gRenderBackground = boolValue;
		}else if(strcmp(key, "enable_fog") == 0){
			if(parseBoolSetting(value, &boolValue)) gEnableFog = boolValue;
		}else if(strcmp(key, "enable_timecycle_boxes") == 0){
			if(parseBoolSetting(value, &boolValue)) gEnableTimecycleBoxes = boolValue;
		}else if(strcmp(key, "gizmo_enabled") == 0){
			if(parseBoolSetting(value, &boolValue)) gGizmoEnabled = boolValue;
		}else if(strcmp(key, "gizmo_mode") == 0){
			parseIntSetting(value, &gGizmoMode);
		}else if(strcmp(key, "gizmo_snap") == 0){
			if(parseBoolSetting(value, &boolValue)) gGizmoSnap = boolValue;
		}else if(strcmp(key, "gizmo_snap_angle") == 0){
			parseFloatSetting(value, &gGizmoSnapAngle);
		}else if(strcmp(key, "gizmo_snap_translate") == 0){
			parseFloatSetting(value, &gGizmoSnapTranslate);
		}else if(strcmp(key, "place_snap_to_objects") == 0){
			if(parseBoolSetting(value, &boolValue)) gPlaceSnapToObjects = boolValue;
		}else if(strcmp(key, "place_snap_to_ground") == 0){
			if(parseBoolSetting(value, &boolValue)) gPlaceSnapToGround = boolValue;
		}else if(strcmp(key, "drag_follow_ground") == 0){
			if(parseBoolSetting(value, &boolValue)) gDragFollowGround = boolValue;
		}else if(strcmp(key, "drag_align_to_surface") == 0){
			if(parseBoolSetting(value, &boolValue)) gDragAlignToSurface = boolValue;
		}else if(strcmp(key, "editor_camera_name") == 0){
			parseQuotedStringValue(value, gEditorCameraName, sizeof(gEditorCameraName));
		}else if(strcmp(key, "editor_model_filter") == 0){
			char buf[256];
			if(parseQuotedStringValue(value, buf, sizeof(buf)))
				setTextFilterValue(gEditorModelFilter, buf);
		}else if(strcmp(key, "editor_txd_filter") == 0){
			char buf[256];
			if(parseQuotedStringValue(value, buf, sizeof(buf)))
				setTextFilterValue(gEditorTxdFilter, buf);
		}else if(strcmp(key, "editor_highlight_matches") == 0){
			if(parseBoolSetting(value, &boolValue)) gEditorHighlightMatches = boolValue;
		}else if(strcmp(key, "browser_selected_category") == 0){
			parseIntSetting(value, &gBrowserSelectedCategory);
		}else if(strcmp(key, "browser_selected_ide") == 0){
			parseQuotedStringValue(value, gBrowserSelectedIde, sizeof(gBrowserSelectedIde));
		}else if(strcmp(key, "browser_active_tab") == 0){
			parseIntSetting(value, &gBrowserActiveTab);
		}else if(strcmp(key, "browser_category_filter") == 0){
			char buf[256];
			if(parseQuotedStringValue(value, buf, sizeof(buf)))
				setTextFilterValue(gBrowserCategoryFilter, buf);
		}else if(strcmp(key, "browser_ide_filter") == 0){
			char buf[256];
			if(parseQuotedStringValue(value, buf, sizeof(buf)))
				setTextFilterValue(gBrowserIdeFilter, buf);
		}else if(strcmp(key, "browser_search_filter") == 0){
			char buf[256];
			if(parseQuotedStringValue(value, buf, sizeof(buf)))
				setTextFilterValue(gBrowserSearchFilter, buf);
		}else if(strcmp(key, "browser_favourites_filter") == 0){
			char buf[256];
			if(parseQuotedStringValue(value, buf, sizeof(buf)))
				setTextFilterValue(gBrowserFavFilter, buf);
		}else if(strcmp(key, "browser_selected_object") == 0){
			parseIntSetting(value, &savedSpawnObjectId);
		}else if(strcmp(key, "diff_filter") == 0){
			parseIntSetting(value, &gDiffFilter);
		}else if(strcmp(key, "water_snap_enabled") == 0){
			if(parseBoolSetting(value, &boolValue)) WaterLevel::gWaterSnapEnabled = boolValue;
		}else if(strcmp(key, "water_snap_size") == 0){
			parseFloatSetting(value, &WaterLevel::gWaterSnapSize);
		}else if(strcmp(key, "water_sub_mode") == 0){
			parseIntSetting(value, &WaterLevel::gWaterSubMode);
		}else if(strcmp(key, "water_create_shape") == 0){
			parseIntSetting(value, &WaterLevel::gWaterCreateShape);
		}else if(strcmp(key, "water_create_z") == 0){
			parseFloatSetting(value, &WaterLevel::gWaterCreateZ);
		}
	}

	sanitizeAutomaticBackupSettings();
	sanitizeCustomImportSettings();
	normalizePersistentSettings();

	RefreshIplVisibilityEntries();
	for(size_t i = 0; i < gSavedIplVisibilityStates.size(); i++){
		for(int j = 0; j < GetIplVisibilityEntryCount(); j++){
			if(strcmp(GetIplVisibilityEntryName(j), gSavedIplVisibilityStates[i].key) == 0){
				SetIplVisibilityEntryVisible(j, gSavedIplVisibilityStates[i].visible);
				break;
			}
		}
	}
	if(savedSpawnObjectId >= 0 && savedSpawnObjectId < NUMOBJECTDEFS && GetObjectDef(savedSpawnObjectId))
		SetSpawnObjectId(savedSpawnObjectId);
	fclose(f);
}

static void
saveCamSettings(void)
{
	FILE *f;

	f = fopenArianeDataWrite("camsettings.txt");
	if(f == nil)
		return;

	for(int i = 0; i < camSettings.size(); i++){
		CamSetting *cam = &camSettings[i];
		fprintf(f, "\"%s\" %f %f %f  %f %f %f  %f  %d %d %d %d  %d\n",
			cam->name,
			cam->pos.x, cam->pos.y, cam->pos.z,
			cam->target.x, cam->target.y, cam->target.z,
			cam->fov,
			cam->hour, cam->minute, cam->weather1, cam->weather2,
			cam->area);
	}

	fclose(f);
}

static void
saveSaveSettings(void)
{
	FILE *f;

	sanitizeAutomaticBackupSettings();
	sanitizeCustomImportSettings();
	UpdateEditorWindowState();
	normalizePersistentSettings();
	f = fopenArianeDataWrite("savesettings.txt");
	if(f == nil)
		return;

	fprintf(f, "window_width %d\n", gSavedWindowWidth);
	fprintf(f, "window_height %d\n", gSavedWindowHeight);
	if(gSavedWindowPlacementValid){
		fprintf(f, "window_x %d\n", gSavedWindowX);
		fprintf(f, "window_y %d\n", gSavedWindowY);
	}
	fprintf(f, "window_maximized %d\n", gSavedWindowMaximized ? 1 : 0);
	fprintf(f, "save_destination %d\n", (int)gSaveDestination);
	fprintf(f, "automatic_backups %d\n", gAutomaticBackupsEnabled ? 1 : 0);
	fprintf(f, "automatic_backup_interval %d\n", gAutomaticBackupIntervalSeconds);
	fprintf(f, "automatic_backup_keep %d\n", gAutomaticBackupKeepCount);
	fprintf(f, "custom_import_start_id %d\n", gCustomImportPreferredStartId);
	fprintf(f, "show_demo_window %d\n", showDemoWindow ? 1 : 0);
	fprintf(f, "show_editor_window %d\n", showEditorWindow ? 1 : 0);
	fprintf(f, "show_instance_window %d\n", showInstanceWindow ? 1 : 0);
	fprintf(f, "show_log_window %d\n", showLogWindow ? 1 : 0);
	fprintf(f, "show_help_window %d\n", showHelpWindow ? 1 : 0);
	fprintf(f, "show_time_weather_window %d\n", showTimeWeatherWindow ? 1 : 0);
	fprintf(f, "show_view_window %d\n", showViewWindow ? 1 : 0);
	fprintf(f, "show_rendering_window %d\n", showRenderingWindow ? 1 : 0);
	fprintf(f, "show_browser_window %d\n", showBrowserWindow ? 1 : 0);
	fprintf(f, "show_diff_window %d\n", showDiffWindow ? 1 : 0);
	fprintf(f, "show_tools_window %d\n", showToolsWindow ? 1 : 0);
	fprintf(f, "toast_enabled %d\n", toastEnabled ? 1 : 0);
	for(int i = 0; i < TOAST_NUM_CATEGORIES; i++)
		fprintf(f, "toast_category_%d %d\n", i, toastCategoryEnabled[i] ? 1 : 0);
	fprintf(f, "time_hour %d\n", currentHour);
	fprintf(f, "time_minute %d\n", currentMinute);
	fprintf(f, "current_area %d\n", currentArea);
	fprintf(f, "weather_old %d\n", Weather::oldWeather);
	fprintf(f, "weather_new %d\n", Weather::newWeather);
	fprintf(f, "weather_interpolation %.9g\n", Weather::interpolation);
	fprintf(f, "extra_colours %d\n", extraColours);
	fprintf(f, "day_night_balance %.9g\n", gDayNightBalance);
	fprintf(f, "wet_road_effect %.9g\n", gWetRoadEffect);
	fprintf(f, "neo_light_map_strength %.9g\n", gNeoLightMapStrength);
	fprintf(f, "camera_position %.9g %.9g %.9g\n", TheCamera.m_position.x, TheCamera.m_position.y, TheCamera.m_position.z);
	fprintf(f, "camera_target %.9g %.9g %.9g\n", TheCamera.m_target.x, TheCamera.m_target.y, TheCamera.m_target.z);
	fprintf(f, "camera_fov %.9g\n", TheCamera.m_fov);
	fprintf(f, "draw_target %d\n", gDrawTarget ? 1 : 0);
	fprintf(f, "render_collision %d\n", gRenderCollision ? 1 : 0);
	fprintf(f, "render_zones %d\n", gRenderZones ? 1 : 0);
	fprintf(f, "render_map_zones %d\n", gRenderMapZones ? 1 : 0);
	fprintf(f, "render_navig_zones %d\n", gRenderNavigZones ? 1 : 0);
	fprintf(f, "render_info_zones %d\n", gRenderInfoZones ? 1 : 0);
	fprintf(f, "render_cull_zones %d\n", gRenderCullZones ? 1 : 0);
	fprintf(f, "render_attrib_zones %d\n", gRenderAttribZones ? 1 : 0);
	fprintf(f, "render_light_effects %d\n", gRenderLightEffects ? 1 : 0);
	fprintf(f, "render_effect_markers %d\n", gRenderEffects ? 1 : 0);
	fprintf(f, "render_legacy_ped_paths %d\n", gRenderLegacyPedPaths ? 1 : 0);
	fprintf(f, "render_legacy_car_paths %d\n", gRenderLegacyCarPaths ? 1 : 0);
	fprintf(f, "render_sa_ped_paths %d\n", gRenderSaPedPaths ? 1 : 0);
	fprintf(f, "render_sa_ped_path_walkers %d\n", gRenderSaPedPathWalkers ? 1 : 0);
	fprintf(f, "render_sa_car_paths %d\n", gRenderSaCarPaths ? 1 : 0);
	fprintf(f, "render_sa_car_path_traffic %d\n", gRenderSaCarPathTraffic ? 1 : 0);
	fprintf(f, "render_sa_area_grid %d\n", gRenderSaAreaGrid ? 1 : 0);
	fprintf(f, "sa_ped_path_walker_count %d\n", gSaPedPathWalkerCount);
	fprintf(f, "sa_car_path_traffic_count %d\n", gSaCarPathTrafficCount);
	fprintf(f, "sa_car_path_traffic_speed_scale %.9g\n", gSaCarPathTrafficSpeedScale);
	fprintf(f, "sa_car_path_traffic_freeze_routes %d\n", gSaCarPathTrafficFreezeRoutes ? 1 : 0);
	fprintf(f, "render_sa_car_path_parked_cars %d\n", gRenderSaCarPathParkedCars ? 1 : 0);
	fprintf(f, "sa_car_path_parked_car_count %d\n", gSaCarPathParkedCarCount);
	fprintf(f, "render_water %d\n", gRenderWater ? 1 : 0);
	fprintf(f, "play_animations %d\n", gPlayAnimations ? 1 : 0);
	fprintf(f, "render_mode %d\n", gRenderMode);
	fprintf(f, "draw_distance %.9g\n", TheCamera.m_LODmult);
	fprintf(f, "render_all_timed_objects %d\n", gNoTimeCull ? 1 : 0);
	fprintf(f, "render_all_areas %d\n", gNoAreaCull ? 1 : 0);
	writeQuotedSetting(f, "ipl_filter_search", gIplFilterSearch);
	for(int i = 0; i < GetIplVisibilityEntryCount(); i++){
		fprintf(f, "ipl_visible ");
		writeInlineQuotedString(f, GetIplVisibilityEntryName(i));
		fprintf(f, " %d\n", GetIplVisibilityEntryVisible(i) ? 1 : 0);
	}
	fprintf(f, "render_postfx %d\n", gRenderPostFX ? 1 : 0);
	fprintf(f, "use_blur_ambient %d\n", gUseBlurAmb ? 1 : 0);
	fprintf(f, "override_blur_ambient %d\n", gOverrideBlurAmb ? 1 : 0);
	fprintf(f, "colour_filter %d\n", gColourFilter);
	fprintf(f, "radiosity %d\n", gRadiosity ? 1 : 0);
	fprintf(f, "building_pipe %d\n", gBuildingPipeSwitch);
	fprintf(f, "backface_culling %d\n", gDoBackfaceCulling ? 1 : 0);
	fprintf(f, "ps2_alpha_test %d\n", params.ps2AlphaTest ? 1 : 0);
	fprintf(f, "alpha_ref %d\n", params.alphaRef);
	fprintf(f, "render_background %d\n", gRenderBackground ? 1 : 0);
	fprintf(f, "enable_fog %d\n", gEnableFog ? 1 : 0);
	fprintf(f, "enable_timecycle_boxes %d\n", gEnableTimecycleBoxes ? 1 : 0);
	fprintf(f, "gizmo_enabled %d\n", gGizmoEnabled ? 1 : 0);
	fprintf(f, "gizmo_mode %d\n", gGizmoMode);
	fprintf(f, "gizmo_snap %d\n", gGizmoSnap ? 1 : 0);
	fprintf(f, "gizmo_snap_angle %.9g\n", gGizmoSnapAngle);
	fprintf(f, "gizmo_snap_translate %.9g\n", gGizmoSnapTranslate);
	fprintf(f, "place_snap_to_objects %d\n", gPlaceSnapToObjects ? 1 : 0);
	fprintf(f, "place_snap_to_ground %d\n", gPlaceSnapToGround ? 1 : 0);
	fprintf(f, "drag_follow_ground %d\n", gDragFollowGround ? 1 : 0);
	fprintf(f, "drag_align_to_surface %d\n", gDragAlignToSurface ? 1 : 0);
	writeQuotedSetting(f, "editor_camera_name", gEditorCameraName);
	writeQuotedSetting(f, "editor_model_filter", gEditorModelFilter.InputBuf);
	writeQuotedSetting(f, "editor_txd_filter", gEditorTxdFilter.InputBuf);
	fprintf(f, "editor_highlight_matches %d\n", gEditorHighlightMatches ? 1 : 0);
	fprintf(f, "browser_selected_category %d\n", gBrowserSelectedCategory);
	writeQuotedSetting(f, "browser_selected_ide", gBrowserSelectedIde);
	fprintf(f, "browser_active_tab %d\n", gBrowserActiveTab);
	writeQuotedSetting(f, "browser_category_filter", gBrowserCategoryFilter.InputBuf);
	writeQuotedSetting(f, "browser_ide_filter", gBrowserIdeFilter.InputBuf);
	writeQuotedSetting(f, "browser_search_filter", gBrowserSearchFilter.InputBuf);
	writeQuotedSetting(f, "browser_favourites_filter", gBrowserFavFilter.InputBuf);
	fprintf(f, "browser_selected_object %d\n", GetSpawnObjectId());
	fprintf(f, "diff_filter %d\n", gDiffFilter);
	fprintf(f, "water_snap_enabled %d\n", WaterLevel::gWaterSnapEnabled ? 1 : 0);
	fprintf(f, "water_snap_size %.9g\n", WaterLevel::gWaterSnapSize);
	fprintf(f, "water_sub_mode %d\n", WaterLevel::gWaterSubMode);
	fprintf(f, "water_create_shape %d\n", WaterLevel::gWaterCreateShape);
	fprintf(f, "water_create_z %.9g\n", WaterLevel::gWaterCreateZ);
	fclose(f);
}

void
SaveEditorSettingsNow(void)
{
	if(!gPersistentSettingsLoaded)
		return;
	saveSaveSettings();
}

static void
getCurrentCamSetting(CamSetting *cam)
{
	for(char *p = cam->name; *p; p++)
		if(*p == '"') *p = ' ';
	cam->pos = TheCamera.m_position;
	cam->target = TheCamera.m_target;
	cam->fov = TheCamera.m_fov;
	cam->hour = currentHour;
	cam->minute = currentMinute;
	cam->weather1 = Weather::oldWeather;
	cam->weather2 = Weather::newWeather;
	cam->area = currentArea;
}

static void
uiEditorWindow(void)
{
	static char buf[256];

	CPtrNode *p;
	ObjectInst *inst;
	ObjectDef *obj;
	TxdDef *txd;

	ImGui::Begin(ICON_FA_PEN " Editor", &showEditorWindow);

	if(ImGui::TreeNode("Camera")){
		ImGui::InputFloat3("Cam position", (float*)&TheCamera.m_position);
		ImGui::InputFloat3("Cam target", (float*)&TheCamera.m_target);
		ImGui::SameLine();
		ImGui::Checkbox("show", &gDrawTarget);
		ImGui::SliderFloat("FOV", (float*)&TheCamera.m_fov, 1.0f, 150.0f, "%.0f");
		ImGui::Text("Far: %f", Timecycle::currentColours.farClp);
		ImGui::Text("mouse: %f %f", TheCamera.mx, TheCamera.my);

		ImGui::InputText("name", gEditorCameraName, sizeof(gEditorCameraName));
		if(ImGui::Button("Save")){
			CamSetting cam;
			strncpy(cam.name, gEditorCameraName, sizeof(cam.name));
			getCurrentCamSetting(&cam);
			camSettings.push_back(cam);
			saveCamSettings();
		}

		for(int i = 0; i < camSettings.size(); i++){
			CamSetting *cam = &camSettings[i];
			ImGui::PushID(i);
			sprintf(buf, "%-20s", cam->name);
			bool del = ImGui::Button("Delete");
			ImGui::SameLine();
			if(ImGui::Button("Replace")){
				strncpy(cam->name, gEditorCameraName, sizeof(cam->name));
				getCurrentCamSetting(cam);
				saveCamSettings();
			}
			ImGui::SameLine();
			if(ImGui::Selectable(buf)){
				strncpy(gEditorCameraName, cam->name, sizeof(gEditorCameraName));
				TheCamera.m_position = cam->pos;
				TheCamera.m_target = cam->target;
				TheCamera.m_fov = cam->fov;
				currentHour = cam->hour;
				currentMinute = cam->minute;
				Weather::oldWeather = cam->weather1;
				Weather::newWeather = cam->weather2;
				if(params.numAreas)
					currentArea = cam->area;
			}
			ImGui::PopID();
			if(del){
				memmove(&camSettings[i], &camSettings[i+1], (camSettings.size()-i-1)*sizeof(CamSetting));
				camSettings.pop_back();
				saveCamSettings();
				i--;
			}
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("CD images")){
		uiShowCdImages();
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("Selection")){
		for(p = selection.first; p; p = p->next){
			inst = (ObjectInst*)p->item;
			obj = GetObjectDef(inst->m_objectId);
			ImGui::PushID(inst);
			ImGui::Selectable(obj->m_name);
			ImGui::PopID();
			if(ImGui::IsItemHovered()){
				inst->m_highlight = HIGHLIGHT_HOVER;
				if(ImGui::IsMouseClicked(1))
					inst->Deselect();
				if(ImGui::IsMouseDoubleClicked(0))
					inst->JumpTo();
			}
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("Instances")){
		gEditorModelFilter.Draw("Model (inc,-exc)"); ImGui::SameLine();
		if(ImGui::Button("Clear##Model"))
			gEditorModelFilter.Clear();
		gEditorTxdFilter.Draw("Txd (inc,-exc)"); ImGui::SameLine();
		if(ImGui::Button("Clear##Txd"))
			gEditorTxdFilter.Clear();
		ImGui::Checkbox("Highlight matches", &gEditorHighlightMatches);
		for(p = instances.first; p; p = p->next){
			inst = (ObjectInst*)p->item;
			obj = GetObjectDef(inst->m_objectId);
			txd = GetTxdDef(obj->m_txdSlot);
			if(gEditorModelFilter.PassFilter(obj->m_name) &&
			   gEditorTxdFilter.PassFilter(txd->name)){
				int numPops = 0;
				if(inst->m_isDeleted){
					ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(128, 128, 128));
					numPops++;
				}else if(inst->m_selected){
					ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(255, 0, 0));
					numPops++;
				}
				ImGui::PushID(inst);
				sprintf(buf, "%s%-20s %-20s %8.2f %8.2f %8.2f",
					inst->m_isDeleted ? "[X] " : "",
					obj->m_name, txd->name,
					inst->m_translation.x, inst->m_translation.y, inst->m_translation.z);
				ImGui::Selectable(buf);
				ImGui::PopID();
				if(ImGui::IsItemHovered()){
					if(ImGui::IsMouseClicked(1)){
						if(inst->m_isDeleted)
							inst->Undelete();
						else
							inst->Select();
					}
					if(ImGui::IsMouseDoubleClicked(0))
						inst->JumpTo();
				}
				if(numPops)
					ImGui::PopStyleColor(numPops);
				if(!inst->m_isDeleted){
					if(gEditorHighlightMatches)
						inst->m_highlight = HIGHLIGHT_FILTER;
					if(ImGui::IsItemHovered())
						inst->m_highlight = HIGHLIGHT_HOVER;
				}
			}
		}
		ImGui::TreePop();
	}

	PathNode *nd;
	if(nd = Path::GetDetachedCarNode(0,0))
	if(ImGui::TreeNode("Detached Legacy Car Paths")){
		for(int i = 0; nd = Path::GetDetachedCarNode(i,0); i++){
			static char str[32];
			sprintf(str, nd->water ? "Legacy Water Path %d" : "Legacy Car Path %d", i);
			ImGui::PushID(i);
			ImGui::Selectable(str);
			ImGui::PopID();
			if(ImGui::IsItemHovered()){
				Path::guiHoveredNode = nd;
				if(ImGui::IsMouseClicked(1))
					Path::selectedNode = nd;
				if(ImGui::IsMouseDoubleClicked(0))
					nd->JumpTo(nil);
			}
		}
		ImGui::TreePop();
	}

	if(nd = Path::GetDetachedPedNode(0,0))
	if(ImGui::TreeNode("Detached Legacy Ped Paths")){
		for(int i = 0; nd = Path::GetDetachedPedNode(i,0); i++){
			static char str[32];
			sprintf(str,"Legacy Ped Path %d", i);
			ImGui::PushID(i);
			ImGui::Selectable(str);
			ImGui::PopID();
			if(ImGui::IsItemHovered()){
				Path::guiHoveredNode = nd;
				if(ImGui::IsMouseClicked(1))
					Path::selectedNode = nd;
				if(ImGui::IsMouseDoubleClicked(0))
					nd->JumpTo(nil);
			}
		}
		ImGui::TreePop();
	}

	ImGui::End();
}

static void
uiToolsWindow(void)
{
	ImGui::Begin(ICON_FA_WRENCH " Tools", &showToolsWindow);

	// Gizmo
	ImGui::Checkbox("Gizmo", &gGizmoEnabled);
	if(gGizmoEnabled){
		ImGui::SameLine();
		if(ImGui::RadioButton("Translate (W)", gGizmoMode == GIZMO_TRANSLATE))
			gGizmoMode = GIZMO_TRANSLATE;
		ImGui::SameLine();
		if(ImGui::RadioButton("Rotate (Q)", gGizmoMode == GIZMO_ROTATE))
			gGizmoMode = GIZMO_ROTATE;

		ImGui::Checkbox("Grid Snap", &gGizmoSnap);
		ImGui::SetItemTooltip("Snap gizmo movements to fixed increments.");
		if(gGizmoSnap){
			char buf[32];
			ImGui::SameLine();
			if(gGizmoMode == GIZMO_ROTATE){
				snprintf(buf, sizeof(buf), "%d\xC2\xB0", (int)gGizmoSnapAngle);
				ImGui::SetNextItemWidth(80);
				if(ImGui::BeginCombo("##snapangle", buf)){
					float angles[] = { 5, 10, 15, 30, 45, 90 };
					for(int i = 0; i < 6; i++){
						bool selected = gGizmoSnapAngle == angles[i];
						snprintf(buf, sizeof(buf), "%d\xC2\xB0", (int)angles[i]);
						if(ImGui::Selectable(buf, selected))
							gGizmoSnapAngle = angles[i];
						if(selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}else{
				snprintf(buf, sizeof(buf), "%d", (int)gGizmoSnapTranslate);
				ImGui::SetNextItemWidth(80);
				if(ImGui::BeginCombo("##snaptrans", buf)){
					float intervals[] = { 1, 2, 5, 10, 25, 50 };
					for(int i = 0; i < 6; i++){
						bool selected = gGizmoSnapTranslate == intervals[i];
						snprintf(buf, sizeof(buf), "%d", (int)intervals[i]);
						if(ImGui::Selectable(buf, selected))
							gGizmoSnapTranslate = intervals[i];
						if(selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}
		}
	}

	ImGui::Separator();

	// Placement
	ImGui::Text("Placement");
	ImGui::Checkbox("Snap to object", &gPlaceSnapToObjects);
	ImGui::SetItemTooltip("When placing objects, snap to the surface of existing objects under the cursor.");
	ImGui::Checkbox("Snap to ground", &gPlaceSnapToGround);
	ImGui::SetItemTooltip("When placing objects, snap to the ground below the cursor.");

	ImGui::Separator();

	// Dragging
	ImGui::Text("Dragging");
	ImGui::Checkbox("Follow ground", &gDragFollowGround);
	ImGui::SetItemTooltip("While dragging objects, keep them glued to the ground surface.");
	ImGui::BeginDisabled(!gDragFollowGround);
	ImGui::Indent();
	ImGui::Checkbox("Align to surface", &gDragAlignToSurface);
	ImGui::SetItemTooltip("While dragging, rotate the object to match the ground slope.");
	ImGui::Unindent();
	ImGui::EndDisabled();

	ImGui::Separator();

	// Automatic backups
	ImGui::Text("Automatic Backups");
	bool backupSettingsChanged = false;
	if(ImGui::Checkbox("Enabled", &gAutomaticBackupsEnabled))
		backupSettingsChanged = true;
	ImGui::SetItemTooltip("Periodically save a backup of all modified IPLs.");
	if(ImGui::InputInt("Interval (sec)", &gAutomaticBackupIntervalSeconds))
		backupSettingsChanged = true;
	ImGui::SetItemTooltip("Seconds between automatic backup snapshots.");
	if(ImGui::InputInt("Keep snapshots", &gAutomaticBackupKeepCount))
		backupSettingsChanged = true;
	ImGui::SetItemTooltip("Number of backup snapshots to keep. Oldest are deleted first.");
	sanitizeAutomaticBackupSettings();
	if(backupSettingsChanged)
		saveSaveSettings();
	ImGui::TextDisabled("Idle debounce: %.0f sec", gAutomaticBackupIdleSeconds);
	if(gAutomaticBackupLastSnapshot[0])
		ImGui::TextWrapped("Last snapshot: %s", gAutomaticBackupLastSnapshot);
	if(ImGui::Button("Create Backup Now"))
		runAutomaticBackup(true);

		ImGui::End();
	}

static bool waterPanelEditActive;

static void
waterPanelCheckUndoPush(void)
{
	if(ImGui::IsItemActivated() && !waterPanelEditActive){
		WaterLevel::WaterUndoPush();
		waterPanelEditActive = true;
	}
	if(!ImGui::IsAnyItemActive())
		waterPanelEditActive = false;
}

static void
uiWaterWindow(void)
{
	if(!ImGui::IsAnyItemActive())
		waterPanelEditActive = false;

	ImGui::Begin("Water Editor", nil);

	// Creation mode UI
	if(WaterLevel::gWaterCreateMode > 0){
		const char *shapeName = WaterLevel::gWaterCreateShape == 0 ? "QUAD" : "TRIANGLE";
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f), "CREATE %s", shapeName);
		ImGui::Separator();
		ImGui::DragFloat("Z Height", &WaterLevel::gWaterCreateZ, 0.5f);
		const char *shapeNames[] = { "Quad (2 clicks)", "Triangle (3 clicks)" };
		int prevShape = WaterLevel::gWaterCreateShape;
		ImGui::Combo("Shape", &WaterLevel::gWaterCreateShape, shapeNames, 2);
		if(WaterLevel::gWaterCreateShape != prevShape){
			// Restart placement with new shape
			WaterLevel::CancelCreateMode();
			WaterLevel::EnterCreateMode();
		}
		ImGui::Checkbox("Snap to Grid", &WaterLevel::gWaterSnapEnabled);
		if(WaterLevel::gWaterSnapEnabled){
			ImGui::SameLine();
			ImGui::SetNextItemWidth(60);
			ImGui::DragFloat("##SnapSize", &WaterLevel::gWaterSnapSize, 1.0f, 1.0f, 100.0f, "%.0f");
		}
		ImGui::Separator();
		int neededCorners = WaterLevel::gWaterCreateShape == 0 ? 2 : 3;
		ImGui::Text("Click corner %d of %d", WaterLevel::gWaterCreateMode, neededCorners);
		ImGui::Text("Shift+click: keep creating after placement");
		ImGui::Text("Right-click or Esc: cancel");
		ImGui::Separator();
		ImGui::Text("Quads: %d/%d  Tris: %d/%d  Verts: %d/%d",
			WaterLevel::GetNumQuads(), NUMWATERQUADS,
			WaterLevel::GetNumTris(), NUMWATERTRIS,
			WaterLevel::GetNumVertices(), NUMWATERVERTICES);
		ImGui::End();
		return;
	}

	const char *modeName = WaterLevel::gWaterSubMode == 0 ? "Polygon" : "Vertex";
	ImGui::Text("Mode: %s (Tab to switch)", modeName);
	ImGui::Separator();

	int numPolySel = WaterLevel::GetNumSelectedPolys();
	int numVertSel = WaterLevel::GetNumSelectedVertices();

	if(WaterLevel::gWaterSubMode == 0){
		// Polygon mode
		if(numPolySel == 0){
			ImGui::Text("Click a water polygon to select it");
			ImGui::Text("Shift+click: add, Ctrl+click: toggle");
			ImGui::Text("N: create new quad");
		}else if(numPolySel == 1){
			int ptype = WaterLevel::GetSelectedPolyType(0);
			int pidx = WaterLevel::GetSelectedPolyIndex(0);
			const char *shapeName = ptype == 0 ? "Quad" : "Triangle";
			ImGui::Text("Water %s #%d", shapeName, pidx);

			// Flags
			int *flagsPtr;
			int numVerts;
			int *indices;
			if(ptype == 0){
				WaterLevel::WaterQuad *q = WaterLevel::GetQuad(pidx);
				flagsPtr = &q->flags;
				numVerts = 4;
				indices = q->indices;
			}else{
				WaterLevel::WaterTri *t = WaterLevel::GetTri(pidx);
				flagsPtr = &t->flags;
				numVerts = 3;
				indices = t->indices;
			}

			bool visible = (*flagsPtr & 1) != 0;
			bool limited = (*flagsPtr & 2) != 0;
			if(ImGui::Checkbox("Visible", &visible)){
				WaterLevel::WaterUndoPush();
				*flagsPtr = (*flagsPtr & ~1) | (visible ? 1 : 0);
				WaterLevel::gWaterDirty = true;
			}
			ImGui::SameLine();
			if(ImGui::Checkbox("Limited Depth", &limited)){
				WaterLevel::WaterUndoPush();
				*flagsPtr = (*flagsPtr & ~2) | (limited ? 2 : 0);
				WaterLevel::gWaterDirty = true;
			}

			ImGui::Separator();

			// Per-vertex properties
			for(int j = 0; j < numVerts; j++){
				WaterLevel::WaterVertex *v = WaterLevel::GetVertex(indices[j]);
				ImGui::PushID(j);
				char label[32];
				snprintf(label, sizeof(label), "Vertex %d", j);
				if(ImGui::TreeNode(label)){
					bool changed = false;
					rw::V3d oldPos = v->pos;
					changed |= ImGui::DragFloat3("Position", (float*)&v->pos, 0.1f);
					waterPanelCheckUndoPush();
					changed |= ImGui::DragFloat2("Flow", (float*)&v->speed, 0.01f, -2.0f, 1.984375f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
					waterPanelCheckUndoPush();
					changed |= ImGui::DragFloat("Big waves", &v->waveunk, 0.01f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
					waterPanelCheckUndoPush();
					changed |= ImGui::DragFloat("Small waves", &v->waveheight, 0.01f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
					waterPanelCheckUndoPush();
					if(changed){
						WaterLevel::WeldCoincidentVertices(indices[j], oldPos);
						WaterLevel::gWaterDirty = true;
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			ImGui::Separator();

			// Flatten Z button
			if(ImGui::Button("Flatten Z")){
				WaterLevel::WaterUndoPush();
				float avgZ = 0.0f;
				for(int j = 0; j < numVerts; j++)
					avgZ += WaterLevel::GetVertex(indices[j])->pos.z;
				avgZ /= (float)numVerts;
				for(int j = 0; j < numVerts; j++){
					rw::V3d op = WaterLevel::GetVertex(indices[j])->pos;
					WaterLevel::GetVertex(indices[j])->pos.z = avgZ;
					WaterLevel::WeldCoincidentVertices(indices[j], op);
				}
				WaterLevel::gWaterDirty = true;
			}

			// Set Z
			static float setZValue = 0.0f;
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80);
			ImGui::DragFloat("##SetZ", &setZValue, 0.1f);
			ImGui::SameLine();
			if(ImGui::Button("Set Z")){
				WaterLevel::WaterUndoPush();
				for(int j = 0; j < numVerts; j++){
					rw::V3d op = WaterLevel::GetVertex(indices[j])->pos;
					WaterLevel::GetVertex(indices[j])->pos.z = setZValue;
					WaterLevel::WeldCoincidentVertices(indices[j], op);
				}
				WaterLevel::gWaterDirty = true;
			}
		}else{
			// Multiple polygons selected
			ImGui::Text("%d polygons selected", numPolySel);
			ImGui::Separator();

			// Bulk set Z
			static float bulkZ = 0.0f;
			ImGui::DragFloat("Z value", &bulkZ, 0.1f);
			if(ImGui::Button("Set All Z")){
				WaterLevel::WaterUndoPush();
				for(int i = 0; i < numPolySel; i++){
					int pt = WaterLevel::GetSelectedPolyType(i);
					int pi = WaterLevel::GetSelectedPolyIndex(i);
					int n; int *idx;
					if(pt == 0){
						n = 4; idx = WaterLevel::GetQuad(pi)->indices;
					}else{
						n = 3; idx = WaterLevel::GetTri(pi)->indices;
					}
					for(int j = 0; j < n; j++){
						rw::V3d op = WaterLevel::GetVertex(idx[j])->pos;
						WaterLevel::GetVertex(idx[j])->pos.z = bulkZ;
						WaterLevel::WeldCoincidentVertices(idx[j], op);
					}
				}
				WaterLevel::gWaterDirty = true;
			}
		}
	}else{
		// Vertex mode
		if(numVertSel == 0){
			ImGui::Text("Click a vertex to select it");
			ImGui::Text("Shift+click: add, Ctrl+click: toggle");
		}else if(numVertSel == 1){
			int vi = WaterLevel::GetSelectedVertexIndex(0);
			WaterLevel::WaterVertex *v = WaterLevel::GetVertex(vi);
			ImGui::Text("Water Vertex #%d", vi);
			ImGui::Separator();
			bool changed = false;
			rw::V3d oldPos = v->pos;
			changed |= ImGui::DragFloat3("Position", (float*)&v->pos, 0.1f);
			waterPanelCheckUndoPush();
			changed |= ImGui::DragFloat2("Flow", (float*)&v->speed, 0.01f, -2.0f, 1.984375f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			waterPanelCheckUndoPush();
			changed |= ImGui::DragFloat("Big waves", &v->waveunk, 0.01f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			waterPanelCheckUndoPush();
			changed |= ImGui::DragFloat("Small waves", &v->waveheight, 0.01f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			waterPanelCheckUndoPush();
			if(changed){
				WaterLevel::WeldCoincidentVertices(vi, oldPos);
				WaterLevel::gWaterDirty = true;
			}
		}else{
			ImGui::Text("%d vertices selected", numVertSel);
			ImGui::Separator();
			static float bulkZ = 0.0f;
			ImGui::DragFloat("Z value", &bulkZ, 0.1f);
			if(ImGui::Button("Set All Z")){
				WaterLevel::WaterUndoPush();
				for(int i = 0; i < numVertSel; i++){
					int vi = WaterLevel::GetSelectedVertexIndex(i);
					rw::V3d op = WaterLevel::GetVertex(vi)->pos;
					WaterLevel::GetVertex(vi)->pos.z = bulkZ;
					WaterLevel::WeldCoincidentVertices(vi, op);
				}
				WaterLevel::gWaterDirty = true;
			}
		}
	}

	ImGui::Separator();

	// Tools
	ImGui::Checkbox("Snap to Grid", &WaterLevel::gWaterSnapEnabled);
	if(WaterLevel::gWaterSnapEnabled){
		ImGui::SameLine();
		ImGui::SetNextItemWidth(60);
		ImGui::DragFloat("##SnapSz", &WaterLevel::gWaterSnapSize, 1.0f, 1.0f, 100.0f, "%.0f");
	}

	ImGui::Separator();

	// Stats and actions
	int nq = WaterLevel::GetNumQuads(), nt = WaterLevel::GetNumTris(), nv = WaterLevel::GetNumVertices();
	ImGui::Text("Quads: %d/301  Tris: %d/6  Verts: %d", nq, nt, nv);
	if(nq > 301 || nt > 6)
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Exceeds game polygon limits!");
	ImGui::TextDisabled("Unique vertex limit (1021) checked on save");
	if(WaterLevel::gWaterDirty)
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Unsaved changes (Ctrl+S to save)");
	if(WaterLevel::WaterCanUndo()) ImGui::SameLine();
	if(WaterLevel::WaterCanUndo() && ImGui::SmallButton("Undo"))
		WaterLevel::WaterUndo();
	if(WaterLevel::WaterCanRedo()) ImGui::SameLine();
	if(WaterLevel::WaterCanRedo() && ImGui::SmallButton("Redo"))
		WaterLevel::WaterRedo();

	if(ImGui::Button("Reload water.dat")){
		WaterLevel::ReloadWater();
		Toast(TOAST_SAVE, "Reloaded water.dat");
	}

	ImGui::End();

}

static void
uiInstWindow(void)
{
	ImGui::Begin(ICON_FA_CIRCLE_INFO " Object Info", &showInstanceWindow);

	if(selection.first){
		int numSelected = 0;
		for(CPtrNode *sel = selection.first; sel; sel = sel->next)
			numSelected++;
		ImGui::Text("%d selected", numSelected);
		char exportDir[1024];
		bool haveExportDir = GetArianeDataPath(exportDir, sizeof(exportDir), "dff-txd-exports");
		if(ImGui::Button("Export DFF")){
			if(!haveExportDir){
				Toast(TOAST_SAVE, "Failed to resolve dff-txd-exports path");
			}else{
				int failed = 0;
				int exported = ExportSelectedDffs(exportDir, &failed);
				if(exported > 0 || failed == 0)
					Toast(TOAST_SAVE, "Exported %d DFF(s) to %s%s", exported, exportDir,
					      failed > 0 ? " (some skipped)" : "");
				else
					Toast(TOAST_SAVE, "Failed to export DFF(s)");
			}
		}
		ImGui::SameLine();
		if(ImGui::Button("Export TXD")){
			if(!haveExportDir){
				Toast(TOAST_SAVE, "Failed to resolve dff-txd-exports path");
			}else{
				int failed = 0;
				int exported = ExportSelectedTxds(exportDir, &failed);
				if(exported > 0 || failed == 0)
					Toast(TOAST_SAVE, "Exported %d TXD(s) to %s%s", exported, exportDir,
					      failed > 0 ? " (some skipped)" : "");
				else
					Toast(TOAST_SAVE, "Failed to export TXD(s)");
			}
		}
		if(haveExportDir)
			ImGui::TextDisabled("%s", exportDir);
		ImGui::Separator();

		ObjectInst *inst = (ObjectInst*)selection.first->item;
		ObjectDef *obj = GetObjectDef(inst->m_objectId);
		if(ImGui::CollapsingHeader("Instance"))
			uiInstInfo(inst);
		if(ImGui::CollapsingHeader("Object"))
			uiObjInfo(obj);
		if(obj->m_numEffects)
			if(ImGui::CollapsingHeader("Effects"))
				uiFxInfo(inst);
		if(obj->m_carPathIndex >=0 || obj->m_pedPathIndex >= 0)
			if(ImGui::CollapsingHeader("Legacy Paths"))
				uiPathInfo(inst);
	}else{
		if(Path::selectedNode)// && Path::selectedNode->isDetached())
		if(ImGui::CollapsingHeader("Legacy Paths"))
			uiPathInfo(nil);
		if(SAPaths::HasInfoToShow()){
			if(gSaNodeJustSelected)
				ImGui::SetNextItemOpen(true);
			if(ImGui::CollapsingHeader("San Andreas Streamed Paths"))
				SAPaths::DrawInfoPanel();
		}

/*
		if(Effects::selectedEffect)
		if(ImGui::CollapsingHeader("Effects"))
			uiFxInfo(nil);
*/
	}
	ImGui::End();
}

static void
uiTest(void)
{
	ImGuiContext &g = *GImGui;
	int y = g.FontSizeBase + g.Style.FramePadding.y * 2.0f;	// height of main menu
	ImGui::SetNextWindowPos(ImVec2(0, y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(200, sk::globals.height-y), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin("Dock", nil, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
	ImGui::Text("hi there");
	if(ImGui::IsWindowFocused())
		ImGui::Text("focus");
	if(ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		ImGui::Text("drag");
	if(ImGui::IsWindowHovered())
		ImGui::Text("hover");
	ImGui::End();
	ImGui::PopStyleVar();
}

// Helper: check if category index is or is a child of parent
static bool
isCategoryOrChild(int cat, int parent)
{
	if(cat == parent) return true;
	// Check if cat's parent chain leads to parent
	if(cat >= 0 && cat < NUM_OBJ_CATEGORIES){
		int p = objCategories[cat].parent;
		if(p == parent) return true;
		// Only 2 levels deep max
		if(p >= 0 && objCategories[p].parent == parent) return true;
	}
	return false;
}

// Helper: build indented category name for dropdown
static void
buildCategoryLabel(int idx, char *buf, int bufsize)
{
	int depth = 0;
	if(objCategories[idx].parent >= 0){
		depth = 1;
		if(objCategories[objCategories[idx].parent].parent >= 0)
			depth = 2;
	}
	char prefix[16] = "";
	for(int d = 0; d < depth; d++) strcat(prefix, "  ");
	snprintf(buf, bufsize, "%s%s", prefix, objCategories[idx].name);
}

static void
selectBrowserObject(int i)
{
	SetSpawnObjectId(i);
	RequestObject(i);
	int lodId = GetLodForObject(i);
	if(lodId >= 0) RequestObject(lodId);
}

// Shared object list renderer with clipper
static void
uiObjectList(int *filtered, int numFiltered, int selId)
{
	ImGui::BeginChild("##ObjList", ImVec2(0, 0), true);
	ImGuiListClipper clipper;
	clipper.Begin(numFiltered);
	static char buf[256];
	while(clipper.Step()){
		for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++){
			int i = filtered[row];
			ObjectDef *obj = GetObjectDef(i);
			bool isSelected = (i == selId);
			sprintf(buf, "%5d  %s", obj->m_id, obj->m_name);

			ImGui::PushID(i);
			if(ImGui::Selectable(buf, isSelected))
				selectBrowserObject(i);
			// Right-click for favourites
			if(ImGui::BeginPopupContextItem()){
				if(IsFavourite(i)){
					if(ImGui::MenuItem("Remove from Favourites"))
						ToggleFavourite(i);
				}else{
					if(ImGui::MenuItem("Add to Favourites"))
						ToggleFavourite(i);
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
}

static void
uiBrowserWindow(void)
{
	ImGui::SetNextWindowSize(ImVec2(420, 700), ImGuiCond_FirstUseEver);
	ImGui::Begin(ICON_FA_MAGNIFYING_GLASS " Object Browser", &showBrowserWindow);

	int selId = GetSpawnObjectId();
	static int filtered[NUMOBJECTDEFS];
	int numFiltered = 0;

	// 3D Preview + selected info panel
	if(selId >= 0){
		ObjectDef *sel = GetObjectDef(selId);
		if(sel){
			// Preview (rendered in Draw() before main camera)
			if(gPreviewTexture && gPreviewTexture->raster){
				float previewW = ImGui::GetContentRegionAvail().x;
				float previewH = previewW * 0.75f;
				if(previewH > 200.0f) previewH = 200.0f;
				ImGui::Image((void*)(intptr_t)gPreviewTexture,
					ImVec2(previewW, previewH),
					ImVec2(0, 1), ImVec2(1, 0));
			}

			// Info line
			ImGui::TextColored(ImVec4(0,1,0,1), "%s (ID: %d)", sel->m_name, sel->m_id);
			ImGui::SameLine();
			ImGui::TextDisabled("%.0f", sel->GetLargestDrawDist());
			int lodId = GetLodForObject(selId);
			if(lodId >= 0){
				ObjectDef *lod = GetObjectDef(lodId);
				if(lod){
					ImGui::SameLine();
					ImGui::TextDisabled("LOD: %s", lod->m_name);
				}
			}

			// Action buttons
			if(gPlaceMode){
				if(ImGui::Button("Exit Place Mode"))
					SpawnExitPlaceMode();
			}else{
				if(ImGui::Button("Place"))
					gPlaceMode = true;
			}
			ImGui::SameLine();
			if(IsFavourite(selId)){
				if(ImGui::Button("Unfavourite"))
					ToggleFavourite(selId);
			}else{
				if(ImGui::Button("Favourite"))
					ToggleFavourite(selId);
			}
			ImGui::Separator();
		}
	}

	// Tab bar
	if(ImGui::BeginTabBar("##BrowserTabs")){

		// === Categories tab ===
		if(ImGui::BeginTabItem("Categories", nil,
		   gBrowserTabRestorePending && gBrowserActiveTab == BROWSER_TAB_CATEGORIES ?
		   ImGuiTabItemFlags_SetSelected : 0)){
			gBrowserActiveTab = BROWSER_TAB_CATEGORIES;
			// Category dropdown
			char catLabel[128];
			if(gBrowserSelectedCategory >= 0 && gBrowserSelectedCategory < NUM_OBJ_CATEGORIES)
				snprintf(catLabel, sizeof(catLabel), "%s", objCategories[gBrowserSelectedCategory].name);
			else
				snprintf(catLabel, sizeof(catLabel), "All Categories");
			if(ImGui::BeginCombo("##CatCombo", catLabel)){
				if(ImGui::Selectable("All Categories", gBrowserSelectedCategory == -1)){
					gBrowserSelectedCategory = -1;
				}
				static char lb[128];
				for(int c = 0; c < NUM_OBJ_CATEGORIES; c++){
					buildCategoryLabel(c, lb, sizeof(lb));
					bool isSel = (c == gBrowserSelectedCategory);
					if(ImGui::Selectable(lb, isSel)){
						gBrowserSelectedCategory = c;
					}
				}
				ImGui::EndCombo();
			}

			gBrowserCategoryFilter.Draw("Filter##Cat");

			// Build filtered list
			numFiltered = 0;
			for(int i = 0; i < NUMOBJECTDEFS; i++){
				ObjectDef *obj = GetObjectDef(i);
				if(obj == nil) continue;
				if(gBrowserSelectedCategory >= 0){
					int cat = GetObjectCategory(i);
					if(cat < 0 || !isCategoryOrChild(cat, gBrowserSelectedCategory))
						continue;
				}
				if(!gBrowserCategoryFilter.PassFilter(obj->m_name)) continue;
				filtered[numFiltered++] = i;
			}
			ImGui::Text("%d objects", numFiltered);
			uiObjectList(filtered, numFiltered, selId);

			ImGui::EndTabItem();
		}

		// === IDE tab ===
		if(ImGui::BeginTabItem("IDE", nil,
		   gBrowserTabRestorePending && gBrowserActiveTab == BROWSER_TAB_IDE ?
		   ImGuiTabItemFlags_SetSelected : 0)){
			gBrowserActiveTab = BROWSER_TAB_IDE;
			// Collect unique IDE file names
			static const char *ideFiles[512];
			static int numIdeFiles = 0;
			if(gBrowserIdeListDirty){
				numIdeFiles = 0;
				for(int i = 0; i < NUMOBJECTDEFS; i++){
					ObjectDef *obj = GetObjectDef(i);
					if(obj == nil || obj->m_file == nil) continue;
					bool found = false;
					for(int j = 0; j < numIdeFiles; j++)
						if(strcmp(ideFiles[j], obj->m_file->name) == 0){
							found = true; break;
						}
					if(!found && numIdeFiles < 512)
						ideFiles[numIdeFiles++] = obj->m_file->name;
				}
				gBrowserIdeListDirty = false;
			}

			// IDE dropdown
			const char *ideLabel = gBrowserSelectedIde[0] ? gBrowserSelectedIde : "All IDE files";
			if(ImGui::BeginCombo("##IdeCombo", ideLabel)){
				if(ImGui::Selectable("All IDE files", gBrowserSelectedIde[0] == '\0'))
					gBrowserSelectedIde[0] = '\0';
				for(int j = 0; j < numIdeFiles; j++){
					bool isSel = strcmp(gBrowserSelectedIde, ideFiles[j]) == 0;
					if(ImGui::Selectable(ideFiles[j], isSel))
						snprintf(gBrowserSelectedIde, sizeof(gBrowserSelectedIde), "%s", ideFiles[j]);
				}
				ImGui::EndCombo();
			}

			gBrowserIdeFilter.Draw("Filter##Ide");

			numFiltered = 0;
			for(int i = 0; i < NUMOBJECTDEFS; i++){
				ObjectDef *obj = GetObjectDef(i);
				if(obj == nil) continue;
				if(gBrowserSelectedIde[0] != '\0' &&
				   (obj->m_file == nil || strcmp(obj->m_file->name, gBrowserSelectedIde) != 0))
					continue;
				if(!gBrowserIdeFilter.PassFilter(obj->m_name)) continue;
				filtered[numFiltered++] = i;
			}
			ImGui::Text("%d objects", numFiltered);
			uiObjectList(filtered, numFiltered, selId);

			ImGui::EndTabItem();
		}

		// === Search tab ===
		if(ImGui::BeginTabItem("Search", nil,
		   gBrowserTabRestorePending && gBrowserActiveTab == BROWSER_TAB_SEARCH ?
		   ImGuiTabItemFlags_SetSelected : 0)){
			gBrowserActiveTab = BROWSER_TAB_SEARCH;
			gBrowserSearchFilter.Draw("Search##All");
			ImGui::SameLine();
			if(ImGui::Button("Clear##SearchClear"))
				gBrowserSearchFilter.Clear();

			numFiltered = 0;
			if(gBrowserSearchFilter.IsActive()){
				for(int i = 0; i < NUMOBJECTDEFS; i++){
					ObjectDef *obj = GetObjectDef(i);
					if(obj == nil) continue;
					if(!gBrowserSearchFilter.PassFilter(obj->m_name)) continue;
					filtered[numFiltered++] = i;
				}
			}
			ImGui::Text("%d results", numFiltered);
			uiObjectList(filtered, numFiltered, selId);

			ImGui::EndTabItem();
		}

		// === Favourites tab ===
		if(ImGui::BeginTabItem("Favourites", nil,
		   gBrowserTabRestorePending && gBrowserActiveTab == BROWSER_TAB_FAVOURITES ?
		   ImGuiTabItemFlags_SetSelected : 0)){
			gBrowserActiveTab = BROWSER_TAB_FAVOURITES;
			gBrowserFavFilter.Draw("Filter##Fav");

			numFiltered = 0;
			for(int i = 0; i < NUMOBJECTDEFS; i++){
				if(!IsFavourite(i)) continue;
				ObjectDef *obj = GetObjectDef(i);
				if(obj == nil) continue;
				if(!gBrowserFavFilter.PassFilter(obj->m_name)) continue;
				filtered[numFiltered++] = i;
			}
			ImGui::Text("%d favourites", numFiltered);
			uiObjectList(filtered, numFiltered, selId);

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
		gBrowserTabRestorePending = false;
	}

	ImGui::End();
}

static void
uiDiffWindow(void)
{
	ImGui::Begin(ICON_FA_CODE_COMPARE " Changes Since Last Save", &showDiffWindow);

	// Count changes by category
	int numAdded = 0, numDeleted = 0, numMoved = 0, numRotated = 0, numRestored = 0;
	for(CPtrNode *p = instances.first; p; p = p->next){
		int flags = GetInstanceDiffFlags((ObjectInst*)p->item);
		if(flags & DIFF_ADDED)    numAdded++;
		if(flags & DIFF_DELETED)  numDeleted++;
		if(flags & DIFF_MOVED)    numMoved++;
		if(flags & DIFF_ROTATED)  numRotated++;
		if(flags & DIFF_RESTORED) numRestored++;
	}

	int total = numAdded + numDeleted + numMoved + numRotated + numRestored;
	if(total == 0){
		ImGui::TextDisabled("No changes since last save.");
		ImGui::End();
		return;
	}

	ImGui::Text("%d added, %d deleted, %d moved, %d rotated", numAdded, numDeleted, numMoved, numRotated);
	if(numRestored > 0)
		ImGui::SameLine(), ImGui::Text(", %d restored", numRestored);
	ImGui::Separator();

	// Filter buttons (bitmask — 0 = show all)
	if(ImGui::RadioButton("All", gDiffFilter == 0)) gDiffFilter = 0;
	ImGui::SameLine();
	if(ImGui::RadioButton("Added", gDiffFilter == DIFF_ADDED)) gDiffFilter = DIFF_ADDED;
	ImGui::SameLine();
	if(ImGui::RadioButton("Deleted", gDiffFilter == DIFF_DELETED)) gDiffFilter = DIFF_DELETED;
	ImGui::SameLine();
	if(ImGui::RadioButton("Moved", gDiffFilter == DIFF_MOVED)) gDiffFilter = DIFF_MOVED;
	ImGui::SameLine();
	if(ImGui::RadioButton("Rotated", gDiffFilter == DIFF_ROTATED)) gDiffFilter = DIFF_ROTATED;
	if(numRestored > 0){
		ImGui::SameLine();
		if(ImGui::RadioButton("Restored", gDiffFilter == DIFF_RESTORED)) gDiffFilter = DIFF_RESTORED;
	}

	// Collect changed instances into temp array for sorting
	ObjectInst *changed[4096];
	int numChanged = 0;
	for(CPtrNode *p = instances.first; p; p = p->next){
		ObjectInst *inst = (ObjectInst*)p->item;
		int flags = GetInstanceDiffFlags(inst);
		if(flags == 0) continue;
		if(gDiffFilter != 0 && !(flags & gDiffFilter)) continue;
		if(numChanged < 4096)
			changed[numChanged++] = inst;
	}

	// Sort by m_changeSeq descending (most recent first)
	for(int i = 0; i < numChanged - 1; i++)
		for(int j = i + 1; j < numChanged; j++)
			if(changed[j]->m_changeSeq > changed[i]->m_changeSeq){
				ObjectInst *tmp = changed[i];
				changed[i] = changed[j];
				changed[j] = tmp;
			}

	ImGui::BeginChild("DiffList", ImVec2(0, 0), true);
	for(int i = 0; i < numChanged; i++){
		ObjectInst *inst = changed[i];
		int flags = GetInstanceDiffFlags(inst);
		ObjectDef *obj = GetObjectDef(inst->m_objectId);
		const char *name = obj ? obj->m_name : "???";

		// Build prefix string from flags
		char prefix[8] = "";
		ImVec4 color = ImVec4(1,1,1,1);
		if(flags & DIFF_ADDED){
			strcat(prefix, "+");
			color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
		}else if(flags & DIFF_DELETED){
			strcat(prefix, "-");
			color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
		}else if(flags & DIFF_RESTORED){
			strcat(prefix, "U");
			color = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
		}else{
			if(flags & DIFF_MOVED)  strcat(prefix, "M");
			if(flags & DIFF_ROTATED) strcat(prefix, "R");
			color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
			if(flags == DIFF_ROTATED)
				color = ImVec4(0.3f, 0.7f, 1.0f, 1.0f);
		}

		char buf[256];
		if(flags & DIFF_MOVED){
			float dist = length(sub(inst->m_translation, inst->m_savedTranslation));
			snprintf(buf, sizeof(buf), "%-3s %-20s  %.1f, %.1f, %.1f  (%.1fm)",
				prefix, name,
				inst->m_translation.x, inst->m_translation.y, inst->m_translation.z,
				dist);
		}else if(flags & DIFF_ROTATED){
			float dot = fabsf(inst->m_rotation.x * inst->m_savedRotation.x +
			                   inst->m_rotation.y * inst->m_savedRotation.y +
			                   inst->m_rotation.z * inst->m_savedRotation.z +
			                   inst->m_rotation.w * inst->m_savedRotation.w);
			if(dot > 1.0f) dot = 1.0f;
			float angleDeg = 2.0f * acosf(dot) * (180.0f / 3.14159265f);
			snprintf(buf, sizeof(buf), "%-3s %-20s  %.1f, %.1f, %.1f  (%.1f deg)",
				prefix, name,
				inst->m_translation.x, inst->m_translation.y, inst->m_translation.z,
				angleDeg);
		}else{
			snprintf(buf, sizeof(buf), "%-3s %-20s  %.1f, %.1f, %.1f",
				prefix, name,
				inst->m_translation.x, inst->m_translation.y, inst->m_translation.z);
		}

		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::PushID(inst);
		if(ImGui::Selectable(buf)){
			inst->Select();
		}
		ImGui::PopID();
		ImGui::PopStyleColor();

		if(ImGui::IsItemHovered()){
			inst->m_highlight = HIGHLIGHT_HOVER;
			if(ImGui::IsMouseDoubleClicked(0))
				inst->JumpTo();
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

static ExampleAppLog logwindow;
// TODO: this crashes for me on linux. should figure out how to fix
//void addToLogWindow(const char *fmt, va_list args) { logwindow.AddLog(fmt, args); }
void addToLogWindow(const char *fmt, va_list args) { }

void
gui(void)
{
	static bool show_another_window = false;
	static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	static bool camloaded = false;

	if(!camloaded){
		loadCamSettings();
		loadSaveSettings();
		camloaded = true;
	}

	Path::guiHoveredNode = nil;
	uiMainmenu();
	uiDestroyMapPopup();
	UpdaterDrawGui();
	automaticBackupTick();

	// Ctrl+D duplicate in water mode
	if(WaterLevel::gWaterEditMode && CPad::IsCtrlDown() && CPad::IsKeyJustDown('D')){
		int count = WaterLevel::GetNumSelectedPolys();
		if(count > 0){
			WaterLevel::DuplicateSelectedWaterPolys();
			Toast(TOAST_COPY_PASTE, "Duplicated %d water polygon(s)", count);
		}
	}

	// Copy/Paste (not in water edit mode)
	if(!WaterLevel::gWaterEditMode){
		// Select all
		if(CPad::IsCtrlDown() && CPad::IsKeyJustDown('A')){
			if(!WaterLevel::gWaterEditMode){
				ClearSelection();
				int count = 0;
				for(CPtrNode *p = instances.first; p; p = p->next){
					ObjectInst *inst = (ObjectInst*)p->item;
					if(!inst->m_isDeleted){
						inst->Select();
						count++;
					}
				}
				if(count > 0){
					if(count > 64)
						Toast(TOAST_UNDO_REDO, "Selected %d instance(s) (gizmo limited to 64)", count);
					else
						Toast(TOAST_UNDO_REDO, "Selected %d instance(s)", count);
				}
			}
		}
		if(CPad::IsCtrlDown() && CPad::IsKeyJustDown('C')){
			int before = 0;
			for(CPtrNode *p = selection.first; p; p = p->next) before++;
			CopySelected();
			if(before > 0)
				Toast(TOAST_COPY_PASTE, "Copied %d instance(s)", before);
		}
		if(CPad::IsCtrlDown() && CPad::IsKeyJustDown('V')){
			int before = 0;
			for(CPtrNode *p = instances.first; p; p = p->next) before++;
			PasteClipboard();
			int after = 0;
			for(CPtrNode *p = instances.first; p; p = p->next) after++;
			int pasted = after - before;
			if(pasted > 0)
				Toast(TOAST_COPY_PASTE, "Pasted %d instance(s)", pasted);
		}
	}

	if(!CPad::IsCtrlDown() && CPad::IsKeyJustDown('C')) gUseViewerCam = !gUseViewerCam;

	// Prefabs
	if(CPad::IsCtrlDown() && CPad::IsShiftDown() && CPad::IsKeyJustDown('E')){
		if(selection.first)
			gOpenExportPrefab = true;
	}
	if(CPad::IsCtrlDown() && CPad::IsShiftDown() && CPad::IsKeyJustDown('I')){
		gOpenImportPrefab = true;
	}

	// Gizmo mode shortcuts (not in water edit mode — water gizmo is always translate)
	if(!WaterLevel::gWaterEditMode){
		if(CPad::IsKeyJustDown('W')) gGizmoMode = GIZMO_TRANSLATE;
		if(CPad::IsKeyJustDown('Q')) gGizmoMode = GIZMO_ROTATE;
	}

	// Delete
	if(CPad::IsKeyJustDown(KEY_DEL) || CPad::IsKeyJustDown(KEY_BACKSP)){
		if(WaterLevel::gWaterEditMode){
			int count = WaterLevel::GetNumSelectedPolys();
			if(count > 0){
				WaterLevel::DeleteSelectedWaterPolys();
				Toast(TOAST_DELETE, "Deleted %d water polygon(s)", count);
			}
		}else{
			int count = 0;
			for(CPtrNode *p = selection.first; p; p = p->next) count++;
			if(count > 0){
				DeleteSelected();
				Toast(TOAST_DELETE, "Deleted %d instance(s)", count);
			}
		}
	}

	// Undo/Redo
	if(CPad::IsCtrlDown() && CPad::IsKeyJustDown('Z')){
		if(WaterLevel::gWaterEditMode){
			if(WaterLevel::WaterCanUndo()){
				WaterLevel::WaterUndo();
				Toast(TOAST_UNDO_REDO, "Water Undo");
			}
		}else{
			Undo();
			Toast(TOAST_UNDO_REDO, "Undo");
		}
	}
	if(CPad::IsCtrlDown() && CPad::IsKeyJustDown('Y')){
		if(WaterLevel::gWaterEditMode){
			if(WaterLevel::WaterCanRedo()){
				WaterLevel::WaterRedo();
				Toast(TOAST_UNDO_REDO, "Water Redo");
			}
		}else{
			Redo();
			Toast(TOAST_UNDO_REDO, "Redo");
		}
	}

	// Ctrl+S to save
	if(CPad::IsCtrlDown() && CPad::IsKeyJustDown('S')){
		if(WaterLevel::gWaterEditMode){
			if(WaterLevel::gWaterDirty){
				if(WaterLevel::SaveWater())
					Toast(TOAST_SAVE, "Saved water.dat to %s", getSaveDestinationLabel());
			}
		}else{
			if(saveAllIpls())
				Toast(TOAST_SAVE, "Saved all IPL files to %s", getSaveDestinationLabel());
		}
	}

	// Ctrl+G to test in game
	if(CPad::IsCtrlDown() && CPad::IsKeyJustDown('G')){
		testInGame();
	}

	// Ctrl+R to hot reload streaming IPLs in running game
	if(CPad::IsCtrlDown() && CPad::IsKeyJustDown('R')){
		hotReloadIpls();
	}

	if(!CPad::IsCtrlDown() && CPad::IsKeyJustDown('G'))
		SnapSelectedToGround(CPad::IsShiftDown());

	if(CPad::IsKeyJustDown('T')) showTimeWeatherWindow ^= 1;
	if(showTimeWeatherWindow){
		ImGui::Begin(ICON_FA_CLOUD_SUN " Time & Weather", &showTimeWeatherWindow);
		uiTimeWeather();
		ImGui::End();
	}

	if(!CPad::IsCtrlDown() && CPad::IsKeyJustDown('V')) showViewWindow ^= 1;
	if(showViewWindow){
		ImGui::SetNextWindowSize(ImVec2(460.0f, 640.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin(ICON_FA_EYE " View", &showViewWindow);
		uiView();
		ImGui::End();
	}

	if(!CPad::IsCtrlDown() && CPad::IsKeyJustDown('R')) showRenderingWindow ^= 1;
	if(showRenderingWindow){
		ImGui::Begin(ICON_FA_PAINTBRUSH " Rendering", &showRenderingWindow);
		uiRendering();
		ImGui::End();
	}

	if(CPad::IsKeyJustDown('X')) showToolsWindow ^= 1;
	if(showToolsWindow) uiToolsWindow();

	{
		static SAPaths::Node *prevSaNode = nil;
		gSaNodeJustSelected = SAPaths::selectedNode != nil && SAPaths::selectedNode != prevSaNode;
		prevSaNode = SAPaths::selectedNode;
		if(gSaNodeJustSelected)
			showInstanceWindow = true;
	}
	if(!CPad::IsCtrlDown() && !CPad::IsShiftDown() && CPad::IsKeyJustDown('I')) showInstanceWindow ^= 1;
	if(showInstanceWindow) uiInstWindow();

	if(!CPad::IsCtrlDown() && !CPad::IsShiftDown() && CPad::IsKeyJustDown('E')) showEditorWindow ^= 1;
	if(showEditorWindow) uiEditorWindow();

	if(!CPad::IsCtrlDown() && CPad::IsKeyJustDown('F')) showDiffWindow ^= 1;
	if(showDiffWindow) uiDiffWindow();

	if(CPad::IsKeyJustDown('B')){
		showBrowserWindow ^= 1;
		if(!showBrowserWindow && gPlaceMode)
			SpawnExitPlaceMode();
	}
	if(showBrowserWindow){
		uiBrowserWindow();
		// ImGui X button can set showBrowserWindow to false
		if(!showBrowserWindow && gPlaceMode)
			SpawnExitPlaceMode();
	}

	// Escape: cancel creation, exit water mode, or exit place mode
	if(CPad::IsKeyJustDown(KEY_ESC)){
		if(WaterLevel::gWaterCreateMode > 0){
			WaterLevel::CancelCreateMode();
		}else if(WaterLevel::gWaterEditMode){
			WaterLevel::gWaterEditMode = false;
			WaterLevel::ClearWaterSelection();
			WaterLevel::gWaterSubMode = 0;
		}else if(gPlaceMode)
			SpawnExitPlaceMode();
	}

	// H toggles water edit mode (SA only)
	if(params.water == GAME_SA && CPad::IsKeyJustDown('H') && !CPad::IsCtrlDown()){
		WaterLevel::gWaterEditMode = !WaterLevel::gWaterEditMode;
		if(WaterLevel::gWaterEditMode){
			ClearSelection();
			if(gPlaceMode)
				SpawnExitPlaceMode();
		}else{
			WaterLevel::CancelCreateMode();
			WaterLevel::ClearWaterSelection();
			WaterLevel::gWaterSubMode = 0;
		}
	}

	// Tab switches water sub-mode (cancel creation first)
	if(WaterLevel::gWaterEditMode && CPad::IsKeyJustDown(KEY_TAB)){
		WaterLevel::CancelCreateMode();
		if(WaterLevel::gWaterSubMode == 0){
			WaterLevel::ClearWaterPolySelection();
			WaterLevel::gWaterSubMode = 1;
		}else{
			WaterLevel::ClearWaterVertexSelection();
			WaterLevel::gWaterSubMode = 0;
		}
	}

	// N enters quad creation mode
	if(WaterLevel::gWaterEditMode && !WaterLevel::gWaterCreateMode && CPad::IsKeyJustDown('N') && !CPad::IsCtrlDown()){
		WaterLevel::EnterCreateMode();
	}

	// Water editor window
	if(WaterLevel::gWaterEditMode)
		uiWaterWindow();

	if(showHelpWindow) uiHelpWindow();
	if(showDemoWindow){
		ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
		ImGui::ShowDemoWindow(&showDemoWindow);
	}

	if(showLogWindow) logwindow.Draw("Log", &showLogWindow);

	// Place mode overlay
	if(gPlaceMode && GetSpawnObjectId() >= 0){
		ObjectDef *obj = GetObjectDef(GetSpawnObjectId());
		if(obj){
			ImGui::SetNextWindowPos(ImVec2(10, ImGui::GetIO().DisplaySize.y - 40));
			ImGui::SetNextWindowBgAlpha(0.6f);
			ImGui::Begin("##PlaceMode", nil,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoFocusOnAppearing);
			ImGui::TextColored(ImVec4(1,1,0,1),
				"PLACE: %s  [Click=Place | Shift+Click=Multi | RMB/Esc=Cancel]", obj->m_name);
			ImGui::End();
		}
	}

	// Water hover hint (when not in water edit mode, mouse over water)
	if(!WaterLevel::gWaterEditMode && !gPlaceMode && params.water == GAME_SA){
		ImGuiIO &hintIO = ImGui::GetIO();
		if(!hintIO.WantCaptureMouse){
			Ray ray;
			ray.start = TheCamera.m_position;
			ray.dir = normalize(TheCamera.m_mouseDir);
			float waterT = 1.0e30f;
			int hit = WaterLevel::PickWaterPoly(ray, &waterT);
			bool showHint = hit != INT_MIN && waterT < 750.0f;
			if(showHint){
				float sceneT = 1.0e30f;
				if(GetVisibleInstUnderRay(ray, nil, &sceneT) && sceneT + 0.5f < waterT)
					showHint = false;
			}
			if(showHint){
				ImGui::SetNextWindowPos(ImVec2(hintIO.MousePos.x + 15, hintIO.MousePos.y + 15));
				ImGui::SetNextWindowBgAlpha(0.7f);
				ImGui::Begin("##WaterHint", nil,
					ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
					ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
					ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs);
				ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "H - Edit Water");
				ImGui::End();
			}
		}
	}

	// Water edit mode overlay
	if(WaterLevel::gWaterEditMode){
		ImGui::SetNextWindowPos(ImVec2(10, ImGui::GetIO().DisplaySize.y - 40));
		ImGui::SetNextWindowBgAlpha(0.6f);
		ImGui::Begin("##WaterMode", nil,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoFocusOnAppearing);
		if(WaterLevel::gWaterCreateMode > 0){
			const char *shape = WaterLevel::gWaterCreateShape == 0 ? "QUAD" : "TRI";
			int needed = WaterLevel::gWaterCreateShape == 0 ? 2 : 3;
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f),
				"CREATE %s [corner %d/%d] Z=%.1f | RMB/Esc:cancel | Shift+click:multi",
				shape, WaterLevel::gWaterCreateMode, needed, WaterLevel::gWaterCreateZ);
		}else{
			const char *subMode = WaterLevel::gWaterSubMode == 0 ? "polygon" : "vertex";
			ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f),
				"WATER [%s] | H:exit | Tab:mode | N:new | Del:delete | Ctrl+D:dup | Ctrl+Z/Y:undo | Ctrl+S:save", subMode);
		}
		ImGui::End();
	}

	uiToasts();

	gSettingsAutosaveSeconds += ImGui::GetIO().DeltaTime;
	if(gSettingsAutosaveSeconds >= 1.0f){
		saveSaveSettings();
		gSettingsAutosaveSeconds = 0.0f;
	}

//	uiTest();
}
