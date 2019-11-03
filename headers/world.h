#ifndef WORLD_H
#define WORLD_H
#include "vector.h"
/*
** Если какой-то из дефайнов ниже
** вырос, удостовериться, что
** int-типа в структуре worldInfo
** хватает для представления всех
** этих значений в степени двойки.
*/
#define WORLD_PROPS_COUNT 10
#define WORLD_COLORS_COUNT 5

enum ColorTypes {
	COLOR_SKY,
	COLOR_CLOUD,
	COLOR_FOG,
	COLOR_AMBIENT,
	COLOR_DIFFUSE
};

enum MapEnvProps {
	PROP_SIDEBLOCK,
	PROP_EDGEBLOCK,
	PROP_EDGELEVEL,
	PROP_CLOUDSLEVEL,
	PROP_FOGDIST,
	PROP_SPDCLOUDS,
	PROP_SPDWEATHER,
	PROP_FADEWEATHER,
	PROP_EXPFOG,
	PROP_SIDEOFFSET
};

enum WeatherTypes {
	WEATHER_SUN,
	WEATHER_RAIN,
	WEATHER_SNOW
};

enum WorldDataType {
	DT_DIM,
	DT_SV,
	DT_SA,
	DT_WT,
	DT_PROPS,
	DT_COLORS,

	DT_END = 0xFF
};

enum ModifiedValues {
	MV_COLORS = 1,
	MV_PROPS = 2,
	MV_TEXPACK = 4,
	MV_WEATHER = 8
};

enum WorldProcesses {
	WP_NOPROC,
	WP_SAVING,
	WP_LOADING,
};

typedef struct worldInfo {
	SVec dimensions;
	Color3 colors[WORLD_COLORS_COUNT];
	int32_t props[WORLD_PROPS_COUNT];
	char texturepack[65];
	Vec spawnVec;
	Ang spawnAng;
	Weather wt;
	uint8_t modval;
	uint16_t modprop;
	uint8_t modclr;
} *WorldInfo;

typedef struct world {
	int32_t id;
	const char* name;
	uint32_t size;
	WorldInfo info;
	bool modified;
	Waitable wait;
	bool loaded;
	bool saveUnload;
	int32_t process;
	BlockID* data;
} *World;

API void Worlds_SaveAll(bool join);

API World World_Create(const char* name);
API void World_AllocBlockArray(World world);
API void World_Free(World world);
API bool World_Add(World world);
API void World_UpdateClients(World world);

API bool World_Load(World world);
API void World_Unload(World world);
API bool World_Save(World world);

API void World_SetDimensions(World world, const SVec* dims);
API bool World_SetBlock(World world, SVec* pos, BlockID id);
API bool World_SetEnvColor(World world, uint8_t type, Color3* color);
API bool World_SetEnvProperty(World world, uint8_t property, int32_t value);
API bool World_SetTexturePack(World world, const char* url);
API bool World_SetWeather(World world, Weather type);

API uint32_t World_GetOffset(World world, SVec* pos);
API BlockID World_GetBlock(World world, SVec* pos);
API int32_t World_GetProperty(World world, uint8_t property);
API Color3* World_GetEnvColor(World world, uint8_t type);
API Weather World_GetWeather(World world);
API World World_GetByName(const char* name);
API World World_GetByID(int32_t id);

VAR World Worlds_List[MAX_WORLDS];
#endif
