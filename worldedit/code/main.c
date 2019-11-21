#include <core.h>
#include <client.h>
#include <command.h>
#include <event.h>
#include <block.h>

#include "main.h"

static cs_uint16 WeAT;

static void CubeNormalize(SVec* s, SVec* e) {
	cs_int16 tmp, *a = (cs_int16*)s, *b = (cs_int16*)e;
	for(int i = 0; i < 3; i++) {
		if(a[i] < b[i]) {
			tmp = a[i];
			a[i] = b[i];
			b[i] = tmp;
		}
		a[i]++;
	}
}

static SVec* GetCuboid(Client client) {
	return Assoc_GetPtr(client, WeAT);
}

static SVec _invalid = {-1, -1, -1};

static void clickhandler(void* param) {
	onPlayerClick a = param;
	if(Client_GetHeldBlock(a->client) != BLOCK_AIR || a->button == 0)
		return;

	SVec* vecs = GetCuboid(a->client);
	if(!vecs) return;

	cs_bool isVecInvalid = Vec_IsInvalid(a->pos);
	if(isVecInvalid && a->button == 2) {
		Vec_Set(vecs[0], -1, -1, -1);
		Client_RemoveSelection(a->client, 0);
	} else if(!isVecInvalid && a->button == 1) {
		if(vecs[0].x == -1)
			vecs[0] = *a->pos;
		else if(!SVec_Compare(&vecs[0], a->pos) && !SVec_Compare(&vecs[1], a->pos)){
			vecs[1] = *a->pos;
			SVec s = vecs[0], e = vecs[1];
			CubeNormalize(&s, &e);
			Client_MakeSelection(a->client, 0, &s, &e, &DefaultSelectionColor);
		}
	}
}

static cs_bool CHandler_Select(CommandCallData ccdata) {
	SVec* ptr = GetCuboid(ccdata->caller);
	if(ptr) {
		Client_RemoveSelection(ccdata->caller, 0);
		Assoc_Remove(ccdata->caller, WeAT, true);
		Command_Print(ccdata, "Selection mode &cdisabled");
	}
	ptr = Memory_Alloc(1, sizeof(SVec) * 2);
	ptr[0] = _invalid;
	ptr[1] = _invalid;
	Assoc_Set(ccdata->caller, WeAT, ptr);
	Command_Print(ccdata, "Selection mode &aenabled");
}

static cs_bool CHandler_Set(CommandCallData ccdata) {
	const char* cmdUsage = "/set <blockid>";
	Client client = ccdata->caller;
	SVec* ptr = GetCuboid(client);
	if(!ptr) {
		Command_Print(ccdata, "Select cuboid first.");
	}

	char blid[4];
	if(!String_GetArgument(ccdata->args, blid, 4, 0)) {
		Command_PrintUsage(ccdata);
	}

	BlockID block = (BlockID)String_ToInt(blid);
	SVec s = ptr[0], e = ptr[1];
	CubeNormalize(&s, &e);
	cs_uint32 count = (s.x - e.x) * (s.y - e.y) * (s.z - e.z);
	struct _BulkBlockUpdate bbu = {0};
	bbu.world = Client_GetWorld(client);
	bbu.autosend = true;

	for(cs_uint16 x = e.x; x < s.x; x++) {
		for(cs_uint16 y = e.y; y < s.y; y++) {
			for(cs_uint16 z = e.z; z < s.z; z++) {
				SVec pos; Vec_Set(pos, x, y, z);
				cs_uint32 offest = World_SetBlock(bbu.world, &pos, block);
				Block_BulkUpdateAdd(&bbu, offest, block);
			}
		}
	}

	Block_BulkUpdateSend(&bbu);

	Command_Printf(ccdata, "%d blocks filled with %d.", count, block);
}

static void freeselvecs(void* param) {
	Assoc_Remove((Client)param, WeAT, true);
}

cs_int32 Plugin_ApiVer = PLUGIN_API_NUM;

cs_bool Plugin_Load(void) {
	WeAT = Assoc_NewType();
	Command_Register("select", CHandler_Select);
	Command_Register("set", CHandler_Set);
	Event_RegisterVoid(EVT_ONCLICK, clickhandler);
	Event_RegisterVoid(EVT_ONDISCONNECT, freeselvecs);
	return true;
}

cs_bool Plugin_Unload(void) {
	Assoc_DelType(WeAT, true);
	Command_UnregisterByName("select");
	Event_Unregister(EVT_ONCLICK, (cs_uintptr)clickhandler);
	Event_Unregister(EVT_ONDISCONNECT, (cs_uintptr)freeselvecs);
	return true;
}