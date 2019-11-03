#include "core.h"
#include "platform.h"
#include "str.h"
#include "client.h"
#include "world.h"
#include "event.h"

void Worlds_SaveAll(bool join) {
	for(int32_t i = 0; i < MAX_WORLDS; i++) {
		World world = Worlds_List[i];

		if(i < MAX_WORLDS && world) {
			if(World_Save(world) && join)
				Waitable_Wait(world->wait);
		}
	}
}

World World_Create(const char* name) {
	World tmp = Memory_Alloc(1, sizeof(struct world));
	WorldInfo wi = Memory_Alloc(1, sizeof(struct worldInfo));

	tmp->name = String_AllocCopy(name);
	tmp->wait = Waitable_Create();
	tmp->process = WP_NOPROC;
	tmp->id = -1;
	tmp->info = wi;

	/*
	** Устанавливаем дефолтные значения
	** согласно документации по CPE.
	*/
	wi->props[PROP_SIDEBLOCK] = 7;
	wi->props[PROP_EDGEBLOCK] = 8;
	wi->props[PROP_FOGDIST] = 0;
	wi->props[PROP_SPDCLOUDS] = 256;
	wi->props[PROP_SPDWEATHER] = 256;
	wi->props[PROP_FADEWEATHER] = 128;
	wi->props[PROP_EXPFOG] = 0;
	wi->props[PROP_SIDEOFFSET] = -2;

	for(int i = 0; i < WORLD_COLORS_COUNT; i++) {
		wi->colors[i].r = -1;
		wi->colors[i].g = -1;
		wi->colors[i].b = -1;
	}

	return tmp;
}

bool World_Add(World world) {
	if(world->id == -1) {
		for(int32_t i = 0; i < MAX_WORLDS; i++) {
			if(!Worlds_List[i]) {
				world->id = i;
				Worlds_List[i] = world;
				return true;
			}
		}
		return false;
	}

	if(world->id > MAX_WORLDS) return false;
	Worlds_List[world->id] = world;
	return true;
}

World World_GetByName(const char* name) {
	for(int32_t i = 0; i < MAX_WORLDS; i++) {
		World world = Worlds_List[i];
		if(world && String_CaselessCompare(world->name, name))
			return world;
	}
	return NULL;
}

World World_GetByID(int32_t id) {
	return id < MAX_WORLDS ? Worlds_List[id] : NULL;
}

void World_SetDimensions(World world, const SVec* dims) {
	world->info->dimensions = *dims;
	world->size = 4 + dims->x * dims->y * dims->z;
}

bool World_SetEnvProperty(World world, uint8_t property, int32_t value) {
	if(property > WORLD_PROPS_COUNT) return false;

	world->modified = true;
	world->info->props[property] = value;
	world->info->modval |= MV_PROPS;
	world->info->modprop |= 2 ^ property;
	return true;
}

int32_t World_GetProperty(World world, uint8_t property) {
	if(property > WORLD_PROPS_COUNT) return 0;
	return world->info->props[property];
}

bool World_SetTexturePack(World world, const char* url) {
	if(String_CaselessCompare(world->info->texturepack, url)) {
		return true;
	}
	world->modified = true;
	world->info->modval |= MV_TEXPACK;
	if(!url || String_Length(url) > 64) {
		world->info->texturepack[0] = '\0';
		return true;
	}
	if(!String_Copy(world->info->texturepack, 65, url)) {
		world->info->texturepack[0] = '\0';
		return false;
	}
	return true;
}

const char* World_GetTexturePack(World world) {
	return world->info->texturepack;
}

bool World_SetWeather(World world, Weather type) {
	if(type > 2) return false;

	world->info->wt = type;
	world->modified = true;
	world->info->modval |= MV_WEATHER;
	Event_Call(EVT_ONWEATHER, world);
	return true;
}

bool World_SetEnvColor(World world, uint8_t type, Color3* color) {
	if(type > WORLD_COLORS_COUNT) return false;
	world->info->modval |= MV_COLORS;
	world->modified = true;
	world->info->colors[type * 3] = *color;
	Event_Call(EVT_ONCOLOR, world);
	return true;
}

Color3* World_GetEnvColor(World world, uint8_t type) {
	if(type > WORLD_COLORS_COUNT) return false;
	return &world->info->colors[type];
}

void World_UpdateClients(World world) {
	Clients_UpdateWorldInfo(world);
	world->info->modval = 0;
}

Weather World_GetWeather(World world) {
	return world->info->wt;
}

void World_AllocBlockArray(World world) {
	BlockID* data = Memory_Alloc(world->size, sizeof(BlockID));
	*(uint32_t*)data = htonl(world->size - 4);
	world->data = data;
	world->loaded = true;
}

void World_Free(World world) {
	if(world->data) Memory_Free(world->data);
	if(world->info) Memory_Free(world->info);
	if(world->id != -1) Worlds_List[world->id] = NULL;
	Memory_Free(world);
}

bool _WriteData(FILE* fp, uint8_t dataType, void* ptr, int32_t size) {
	if(!File_Write(&dataType, 1, 1, fp))
		return false;
	if(ptr && !File_Write(ptr, size, 1, fp))
		return false;
	return true;
}

bool World_WriteInfo(World world, FILE* fp) {
	int32_t magic = WORLD_MAGIC;
	if(!File_Write((char*)&magic, 4, 1, fp)) {
		Error_PrintSys(false);
		return false;
	}
	return _WriteData(fp, DT_DIM, &world->info->dimensions, sizeof(struct _SVec)) &&
	_WriteData(fp, DT_SV, &world->info->spawnVec, sizeof(struct _Vec)) &&
	_WriteData(fp, DT_SA, &world->info->spawnAng, sizeof(struct _Ang)) &&
	_WriteData(fp, DT_WT, &world->info->wt, sizeof(Weather)) &&
	_WriteData(fp, DT_PROPS, world->info->props, 4 * WORLD_PROPS_COUNT) &&
	_WriteData(fp, DT_COLORS, world->info->colors, sizeof(Color3) * WORLD_COLORS_COUNT) &&
	_WriteData(fp, DT_END, NULL, 0);
}

bool World_ReadInfo(World world, FILE* fp) {
	uint8_t id = 0;
	uint32_t magic = 0;
	if(!File_Read(&magic, 4, 1, fp))
		return false;

	if(WORLD_MAGIC != magic) {
		Error_Print2(ET_SERVER, EC_MAGIC, true);
		return false;
	}

	SVec dims = {0};

	while(File_Read(&id, 1, 1, fp) == 1) {
		switch (id) {
			case DT_DIM:
				if(File_Read(&dims, sizeof(struct _SVec), 1, fp) != 1)
					return false;
				World_SetDimensions(world, &dims);
				break;
			case DT_SV:
				if(File_Read(&world->info->spawnVec, sizeof(struct _Vec), 1, fp) != 1)
					return false;
				break;
			case DT_SA:
				if(File_Read(&world->info->spawnAng, sizeof(struct _Ang), 1, fp) != 1)
					return false;
				break;
			case DT_WT:
				if(File_Read(&world->info->wt, sizeof(Weather), 1, fp) != 1)
					return false;
				break;
			case DT_PROPS:
				if(File_Read(world->info->props, 4 * WORLD_PROPS_COUNT, 1, fp) != 1)
					return false;
				break;
			case DT_COLORS:
				if(File_Read(world->info->colors, sizeof(struct _Color3) * WORLD_COLORS_COUNT, 1, fp) != 1)
					return false;
				break;
			case DT_END:
				return true;
			default:
				Error_PrintF2(ET_SERVER, EC_FILECORR, false, world->name);
				return false;
		}
	}

	return false;
}

static TRET wSaveThread(TARG param) {
	World world = param;

	char path[256];
	char tmpname[256];
	String_FormatBuf(path, 256, "worlds/%s", world->name);
	String_FormatBuf(tmpname, 256, "worlds/%s.tmp", world->name);

	FILE* fp = File_Open(tmpname, "w");
	if(!fp) {
		Error_PrintSys(false);
		goto wsdone;
	}

	if(!World_WriteInfo(world, fp)) {
		goto wsdone;
	}

	z_stream stream = {0};
	uint8_t out[1024];
	int32_t ret;

	if((ret = deflateInit(&stream, Z_BEST_COMPRESSION)) != Z_OK) {
		Error_Print2(ET_ZLIB, ret, false);
		goto wsdone;
	}

	stream.avail_in = world->size;
	stream.next_in = (uint8_t*)world->data;

	do {
		stream.next_out = out;
		stream.avail_out = 1024;

		if((ret = deflate(&stream, Z_FINISH)) == Z_STREAM_ERROR) {
			Error_Print2(ET_ZLIB, ret, false);
			goto wsdone;
		}

		if(!File_Write(out, 1, 1024 - stream.avail_out, fp)){
			goto wsdone;
		}
	} while(stream.avail_out == 0);

	File_Rename(tmpname, path);
	if(world->saveUnload)
		World_Free(world);

	wsdone:
	File_Close(fp);
	deflateEnd(&stream);
	Waitable_Signal(world->wait);
	world->process = WP_NOPROC;

	return 0;
}

bool World_Save(World world) {
	if(world->process != WP_NOPROC || !world->modified || !world->loaded)
		return world->process == WP_SAVING;

	world->process = WP_SAVING;
	Thread_Create(wSaveThread, world, true);
	return true;
}

static TRET wLoadThread(TARG param) {
	World world = param;

	int32_t ret = 0;
	char path[256];
	String_FormatBuf(path, 256, "worlds/%s", world->name);

	FILE* fp = File_Open(path, "r");
	if(!fp) {
		Error_PrintSys(false);
		goto wldone;
	}

	if(!World_ReadInfo(world, fp))
		goto wldone;

	World_AllocBlockArray(world);

	z_stream stream = {0};
	uint8_t in[1024];

	if((ret = inflateInit(&stream)) != Z_OK) {
		Error_Print2(ET_ZLIB, ret, false);
		goto wldone;
	}

	stream.next_out = (uint8_t*)world->data;

	do {
		stream.avail_in = (uint32_t)File_Read(in, 1, 1024, fp);
		if(File_Error(fp)) {
			Error_PrintSys(false);
			goto wldone;
		}

		if(stream.avail_in == 0) break;
		stream.next_in = in;

		do {
			stream.avail_out = 1024;
			if((ret = inflate(&stream, Z_NO_FLUSH)) == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
				Error_Print2(ET_ZLIB, ret, false);
				goto wldone;
			}
		} while(stream.avail_out == 0);
	} while(ret != Z_STREAM_END);

	ret = 0;
	wldone:
	File_Close(fp);
	inflateEnd(&stream);
	if(ret != 0)
		World_Unload(world);
	Waitable_Signal(world->wait);
	world->process = WP_NOPROC;
	return 0;
}

bool World_Load(World world) {
	if(world->loaded) return false;
	if(world->process != WP_NOPROC)
		return world->process == WP_LOADING;

	world->process = WP_LOADING;
	Thread_Create(wLoadThread, world, true);
	return true;
}

void World_Unload(World world) {
	if(world->data) Memory_Free(world->data);
	world->loaded = false;
	world->data = NULL;
}

uint32_t World_GetOffset(World world, SVec* pos) {
	WorldInfo wi = world->info;
	SVec* dims = &wi->dimensions;
	uint16_t dx = dims->x, dy = dims->y, dz = dims->z;

	if(pos->x > dx || pos->y > dy || pos->z > dz) return 0;
	return pos->z * dz + pos->y * (dx * dy) + pos->x + 4;
}

bool World_SetBlock(World world, SVec* pos, BlockID id) {
	uint32_t offset = World_GetOffset(world, pos);

	if(offset > 3 && offset < world->size) {
		world->data[offset] = id;
		world->modified = true;
	} else
		return false;

	return true;
}

BlockID World_GetBlock(World world, SVec* pos) {
	uint32_t offset = World_GetOffset(world, pos);

	if(offset > 3 && offset < world->size)
		return world->data[offset];
	else
		return 0;
}
