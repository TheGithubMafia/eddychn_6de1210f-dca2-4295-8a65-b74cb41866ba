#include "core.h"
#include "platform.h"
#include "lang.h"

cs_bool Lang_Init(void) {
	Lang_SwGrp = Lang_NewGroup(2);
	if(!Lang_SwGrp) return false;
	Lang_Set(Lang_SwGrp, 0, "&4disabled");
	Lang_Set(Lang_SwGrp, 1, "&aenabled");

	Lang_ErrGrp = Lang_NewGroup(7);
	if(!Lang_ErrGrp) return false;
	Lang_Set(Lang_ErrGrp, 0, "Unexpected error.");
	Lang_Set(Lang_ErrGrp, 1, "%s:%d in function %s: %s");
	Lang_Set(Lang_ErrGrp, 2, "Invalid packet 0x%02X from Client[%d]");
	Lang_Set(Lang_ErrGrp, 3, "Heartbeat error: %s.");
	Lang_Set(Lang_ErrGrp, 4, "Not a websocket connection.");

	Lang_ConGrp = Lang_NewGroup(8);
	if(!Lang_ConGrp) return false;
	Lang_Set(Lang_ConGrp, 0, "Server started on %s:%d.");
	Lang_Set(Lang_ConGrp, 1, "Last server tick took %dms!");
	Lang_Set(Lang_ConGrp, 2, "Time ran backwards? Time between last ticks < 0.");
	Lang_Set(Lang_ConGrp, 3, "Server play URL: %s.");
	Lang_Set(Lang_ConGrp, 4, "Kicking players...");
	Lang_Set(Lang_ConGrp, 5, "Saving worlds...");
	Lang_Set(Lang_ConGrp, 6, "Plugin \"%s\" is deprecated. Server uses PluginAPI v%03d, but plugin compiled for v%03d.");
	Lang_Set(Lang_ConGrp, 7, "Please upgrade your server software. Plugin \"%s\" compiled for PluginAPI v%03d, but server uses v%d.");

	Lang_KickGrp = Lang_NewGroup(11);
	if(!Lang_KickGrp) return false;
	Lang_Set(Lang_KickGrp, 0, "Kicked without reason");
	Lang_Set(Lang_KickGrp, 1, "Server is full");
	Lang_Set(Lang_KickGrp, 2, "Invalid protocol version");
	Lang_Set(Lang_KickGrp, 3, "This name is already in use");
	Lang_Set(Lang_KickGrp, 4, "Auth failed");
	Lang_Set(Lang_KickGrp, 5, "Server stopped");
	Lang_Set(Lang_KickGrp, 6, "Map sending failed");
	Lang_Set(Lang_KickGrp, 7, "Packet reading error");
	Lang_Set(Lang_KickGrp, 8, "Invalid block ID");
	Lang_Set(Lang_KickGrp, 9, "Too many packets per second");
	Lang_Set(Lang_KickGrp, 10, "Too many connections from one IP");

	Lang_CmdGrp = Lang_NewGroup(18);
	if(!Lang_CmdGrp) return false;
	Lang_Set(Lang_CmdGrp, 0, "Usage: %s.");
	Lang_Set(Lang_CmdGrp, 1, "Access denied.");
	Lang_Set(Lang_CmdGrp, 2, "Player not found.");
	Lang_Set(Lang_CmdGrp, 3, "Unknown command.");
	Lang_Set(Lang_CmdGrp, 4, "This command can't be called from console.");

	Lang_DbgGrp = Lang_NewGroup(2);
	if(!Lang_DbgGrp) return false;
	Lang_Set(Lang_DbgGrp, 0, "Symbol: %s - 0x%0X");
	Lang_Set(Lang_DbgGrp, 1, "\tFile: %s: %d");
	return true;
}

LGroup *Lang_NewGroup(cs_uint32 size) {
	LGroup *grp = Memory_Alloc(1, sizeof(struct _LGroup));
	grp->size = size;
	grp->strings = Memory_Alloc(size, sizeof(cs_str));
	return grp;
}

cs_uint32 Lang_ResizeGroup(LGroup *grp, cs_uint32 newsize) {
	if(grp->size == newsize) return true;
	cs_uint32 oldsize = grp->size;

	cs_str *tmp = Memory_Realloc(
		(void *)grp->strings,
		grp->size * sizeof(cs_str),
		newsize * sizeof(cs_str)
	);

	if(tmp) {
		grp->strings = tmp;
		grp->size = newsize;
		return oldsize;
	}

	return 0;
}

cs_str Lang_Get(LGroup *grp, cs_uint32 id) {
	if(grp && grp->size > id) {
		cs_str pstr = grp->strings[id];
		if(pstr) return pstr;
	}
	return "Invalid langid";
}

cs_bool Lang_Set(LGroup *grp, cs_uint32 id, cs_str str) {
	if(grp->size < id) return false;
	grp->strings[id] = str;
	return true;
}
