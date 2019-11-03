#include "core.h"
#include "str.h"
#include "log.h"
#include "platform.h"
#include "plugin.h"
#include "command.h"
#include "lang.h"

Plugin Plugins_List[MAX_PLUGINS] = {0};

bool Plugin_Load(const char* name) {
	char path[256];
	char error[512];
	String_FormatBuf(path, 256, "plugins/%s", name);
	if(Plugin_Get(name)) return false;
	void *lib, *verSym, *initSym;

	if(DLib_Load(path, &lib)) {
		if(!(DLib_GetSym(lib, "Plugin_ApiVer", &verSym) &&
		DLib_GetSym(lib, "Plugin_Load", &initSym))) {
			Log_Error("%s: %s", path, DLib_GetError(error, 512));
			DLib_Unload(lib);
			return false;
		}

		int32_t ver = *((int32_t*)verSym);
		if(ver != PLUGIN_API_NUM) {
			if(ver < PLUGIN_API_NUM)
				Log_Error(Lang_Get(LANG_CPAPIOLD), name, PLUGIN_API_NUM, ver);
			else
				Log_Error(Lang_Get(LANG_CPAPIUPG), name, ver, PLUGIN_API_NUM);

			DLib_Unload(lib);
			return false;
		}

		Plugin plugin = Memory_Alloc(1, sizeof(struct cPlugin));
		DLib_GetSym(lib, "Plugin_Unload", (void*)&plugin->unload);

		plugin->name = String_AllocCopy(name);
		plugin->lib = lib;
		plugin->id = -1;

		for(int32_t i = 0; i < MAX_PLUGINS; i++) {
			if(!Plugins_List[i]) {
				Plugins_List[i] = plugin;
				plugin->id = i;
				break;
			}
		}

		if(plugin->id == -1 || !(*(pluginFunc)initSym)()) {
			Plugin_Unload(plugin);
			return false;
		}

		return true;
	}

	Log_Error("%s: %s", path, DLib_GetError(error, 512));
	return false;
}

Plugin Plugin_Get(const char* name) {
	for(int32_t i = 0; i < MAX_PLUGINS; i++) {
		Plugin ptr = Plugins_List[i];
		if(ptr && String_Compare(ptr->name, name)) return ptr;
	}
	return NULL;
}

bool Plugin_Unload(Plugin plugin) {
	if(plugin->unload && !(*(pluginFunc)plugin->unload)())
		return false;
	if(plugin->name)
		Memory_Free((void*)plugin->name);
	if(plugin->id != -1)
		Plugins_List[plugin->id] = NULL;

	DLib_Unload(plugin->lib);
	Memory_Free(plugin);
	return true;
}

void Plugin_Start(void) {
	Directory_Ensure("plugins");

	dirIter pIter = {0};
	if(Iter_Init(&pIter, "plugins", DLIB_EXT)) {
		do {
			if(!pIter.isDir && pIter.cfile)
				Plugin_Load(pIter.cfile);
		} while(Iter_Next(&pIter));
	}
}

void Plugin_Stop(void) {
	for(int32_t i = 0; i < MAX_PLUGINS; i++) {
		Plugin plugin = Plugins_List[i];
		if(plugin && plugin->unload)
			(*(pluginFunc)plugin->unload)();
	}
}