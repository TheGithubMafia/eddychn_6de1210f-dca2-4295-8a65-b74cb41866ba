#include "core.h"
#include "world.h"
#include "generators.h"
#include "random.h"

// Генератор плоского мира

void Generator_Flat(World world) {
	WorldInfo wi = world->info;
	uint16_t dx = wi->width,
	dy = wi->height,
	dz = wi->length;

	BlockID* data = world->data + 4;
	int32_t dirtEnd = dx * dz * (dy / 2 - 1);
	for(int32_t i = 0; i < dirtEnd + dx * dz; i++) {
		if(i < dirtEnd)
			data[i] = 3;
		else
			data[i] = 2;
	}

	World_SetProperty(world, PROP_CLOUDSLEVEL, dy + 2);
	World_SetProperty(world, PROP_EDGELEVEL, dy / 2);

	wi->spawnVec.x = (float)dx / 2;
	wi->spawnVec.y = (float)dy / 2;
	wi->spawnVec.z = (float)dz / 2;
}

/*
** Генератор обычного мира.
** Когда-нибудь он точно будет
** готов, но явно не сегодня.
*/

/*
#define MAX_THREADS 16

Thread threads[MAX_THREADS] = {0};
int32_t cfgMaxThreads = 2;

static int32_t AddThread(TFUNC func, TARG arg) {
	for(int32_t i = 0; i < MAX_THREADS; i++) {
		if(i > cfgMaxThreads) {
			i = 0;
			if(Thread_IsValid(threads[i])) {
				Thread_Join(threads[i]);
				threads[i] = NULL;
			}
		}
		if(!Thread_IsValid(threads[i])) {
			threads[i] = Thread_Create(func, arg);
			return i;
		}
	}
	return -1;
}

static void WaitAll(void) {
	for(int32_t i = 0; i < MAX_THREADS; i++) {
		if(Thread_IsValid(threads[i]))
			Thread_Join(threads[i]);
	}
}

void Generator_Default(World world) {
	RNGState rnd;
	Random_Seed(&rnd, 1337);
}
*/
