#include "core.h"
#include "platform.h"
#include "str.h"
#include "block.h"
#include "client.h"
#include "server.h"
#include "packets.h"
#include "event.h"
#include "heartbeat.h"
#include "lang.h"

uint8_t Clients_GetCount(int32_t state) {
	uint8_t count = 0;
	for(ClientID i = 0; i < MAX_CLIENTS; i++) {
		CLIENT client = Clients_List[i];
		if(!client) continue;
		PLAYERDATA pd = client->playerData;
		if(pd && pd->state == state) count++;
	}
	return count;
}

void Clients_KickAll(const char* reason) {
	for(ClientID i = 0; i < MAX_CLIENTS; i++) {
		CLIENT client = Clients_List[i];
		if(client) Client_Kick(client, reason);
	}
}

CLIENT Client_New(SOCKET fd, uint32_t addr) {
	CLIENT tmp = Memory_Alloc(1, sizeof(struct client));
	tmp->id = 0xFF;
	tmp->sock = fd;
	tmp->mutex = Mutex_Create();
	tmp->addr = addr;
	tmp->rdbuf = Memory_Alloc(134, 1);
	tmp->wrbuf = Memory_Alloc(2048, 1);
	return tmp;
}

bool Client_Add(CLIENT client) {
	int8_t maxplayers = Config_GetInt8(Server_Config, CFG_MAXPLAYERS_KEY);
	for(ClientID i = 0; i < min(maxplayers, MAX_CLIENTS); i++) {
		if(!Clients_List[i]) {
			client->id = i;
			client->thread = Thread_Create(Client_ThreadProc, client);
			Clients_List[i] = client;
			return true;
		}
	}
	return false;
}

const char* Client_GetName(CLIENT client) {
	return client->playerData->name;
}

const char* Client_GetAppName(CLIENT client) {
	if(!client->cpeData) return Lang_Get(LANG_CPEVANILLA);
	return client->cpeData->appName;
}

CLIENT Client_GetByName(const char* name) {
	for(ClientID i = 0; i < MAX_CLIENTS; i++) {
		CLIENT client = Clients_List[i];
		if(!client) continue;
		PLAYERDATA pd = client->playerData;
		if(pd && String_CaselessCompare(pd->name, name))
			return client;
	}
	return NULL;
}

CLIENT Client_GetByID(ClientID id) {
	return id < MAX_CLIENTS ? Clients_List[id] : NULL;
}

int16_t Client_GetModel(CLIENT client) {
	return client->cpeData->model;
}

int32_t Client_GetExtVer(CLIENT client, uint32_t extCRC32) {
	CPEDATA cpd = client->cpeData;
	if(!cpd) return false;

	EXT ptr = cpd->firstExtension;
	while(ptr) {
		if(ptr->crc32 == extCRC32) return ptr->version;
		ptr = ptr->next;
	}
	return false;
}

bool Client_Despawn(CLIENT client) {
	PLAYERDATA pd = client->playerData;
	if(!pd || !pd->spawned) return false;
	pd->spawned = false;
	Packet_WriteDespawn(Client_Broadcast, client);
	Event_Call(EVT_ONDESPAWN, (void*)client);
	return true;
}

bool Client_ChangeWorld(CLIENT client, WORLD world) {
	if(Client_IsInWorld(client, world)) return false;

	Client_Despawn(client);
	Client_SetPos(client, world->info->spawnVec, world->info->spawnAng);
	if(!Client_SendMap(client, world)) {
		Client_Kick(client, Lang_Get(LANG_KICKMAPFAIL));
		return false;
	}
	return true;
}

static uint32_t copyMessagePart(const char* message, char* part, uint32_t i, char* color) {
	if(*message == '\0') return 0;

	if(i > 0) {
		*part++ = '>';
		*part++ = ' ';
	}

	if(*color > 0) {
		*part++ = '&';
		*part++ = *color;
	}

	uint32_t len = min(60, (uint32_t)String_Length(message));
	if(message[len - 1] == '&' && ISHEX(message[len])) --len;

	for(uint32_t j = 0; j < len; j++) {
		char prevsym = (*part++ = *message++);
		char nextsym = *message;
		if(nextsym == '\0' || nextsym == '\n') break;
		if(prevsym == '&' && ISHEX(nextsym)) *color = nextsym;
	}

	*part = '\0';
	return len;
}

void Client_Chat(CLIENT client, MessageType type, const char* message) {
	uint32_t msgLen = (uint32_t)String_Length(message);

	if(msgLen > 62 && type == CPE_CHAT) {
		char color = 0, part[65] = {0};
		uint32_t parts = (msgLen / 60) + 1;
		for(uint32_t i = 0; i < parts; i++) {
			uint32_t len = copyMessagePart(message, part, i, &color);
			if(len > 0) {
				Packet_WriteChat(client, type, part);
				message += len;
			}
		}
		return;
	}

	Packet_WriteChat(client, type, message);
}

static void HandlePacket(CLIENT client, char* data, PACKET packet, bool extended) {
	bool ret = false;

	if(extended)
		if(packet->cpeHandler)
			ret = packet->cpeHandler(client, data);
		else
			ret = packet->handler(client, data);
	else
		if(packet->handler)
			ret = packet->handler(client, data);

	if(!ret && !client->closed)
		Client_Kick(client, Lang_Get(LANG_KICKPACKETREAD));
	else
		client->pps += 1;
}

static uint16_t GetPacketSizeFor(PACKET packet, CLIENT client, bool* extended) {
	uint16_t packetSize = packet->size;
	bool _extended = *extended;
	if(packet->haveCPEImp) {
		_extended = Client_GetExtVer(client, packet->extCRC32) == packet->extVersion;
		if(_extended) packetSize = packet->extSize;
	}
	return packetSize;
}

static void PacketReceiverWs(CLIENT client) {
	PACKET packet;
	bool extended;
	uint16_t packetSize, recvSize;
	WSCLIENT ws = client->websock;
	char* data = client->rdbuf;

	if(WsClient_ReceiveFrame(ws)) {
		if(ws->opcode == 0x08) {
			client->closed = true;
			return;
		}

		recvSize = ws->plen - 1;
		handlePacket:
		packet = Packet_Get(*data++);
		if(!packet) {
			Client_Kick(client, Lang_Get(LANG_KICKPACKETREAD));
			return;
		}

		packetSize = GetPacketSizeFor(packet, client, &extended);

		if(packetSize <= recvSize) {
			HandlePacket(client, data, packet, extended);
			/*
				Каждую ~секунду к фрейму с пакетом 0x08 (Teleport)
				приклеивается пакет 0x2B (TwoWayPing) и поскольку
				не исключено, что таких приклеиваний может быть
				много, пришлось использовать goto для обработки
				всех пакетов, входящих в фрейм.
			*/
			if(recvSize > packetSize) {
				data += packetSize;
				recvSize -= packetSize + 1;
				goto handlePacket;
			}

			return;
		} else
			Client_Kick(client, Lang_Get(LANG_KICKPACKETREAD));
	} else
		client->closed = true;
}

static void PacketReceiverRaw(CLIENT client) {
	PACKET packet;
	bool extended;
	uint16_t packetSize;
	uint8_t packetId;

	if(Socket_Receive(client->sock, (char*)&packetId, 1, 0) == 1) {
		packet = Packet_Get(packetId);
		if(!packet) {
			Client_Kick(client, Lang_Get(LANG_KICKPACKETREAD));
			return;
		}

		packetSize = GetPacketSizeFor(packet, client, &extended);

		if(packetSize > 0) {
			int32_t len = Socket_Receive(client->sock, client->rdbuf, packetSize, 0);

			if(packetSize == len)
				HandlePacket(client, client->rdbuf, packet, extended);
			else
				client->closed = true;
		}
	} else
		client->closed = true;
}

TRET Client_ThreadProc(TARG param) {
	CLIENT client = (CLIENT)param;

	while(!client->closed) {
		if(client->websock)
			PacketReceiverWs(client);
		else
			PacketReceiverRaw(client);
	}

	return 0;
}

TRET Client_MapThreadProc(TARG param) {
	CLIENT client = (CLIENT)param;
	if(client->closed) return 0;

	uint8_t* data = (uint8_t*)client->wrbuf;
	PLAYERDATA pd = client->playerData;

	WORLD world = pd->world;
	uint8_t* mapdata = world->data;
	int32_t maplen = world->size;

	*data++ = 0x03;
	uint16_t* len = (uint16_t*)data++;
	uint8_t* out = ++data;

	int32_t ret, windowBits = 31;
	z_stream stream = {0};

	Mutex_Lock(client->mutex);
	if(Client_GetExtVer(client, EXT_FASTMAP)) {
		windowBits = -15;
		maplen -= 4;
		mapdata += 4;
	}

	if((ret = deflateInit2(
		&stream,
		1,
		Z_DEFLATED,
		windowBits,
		8,
		Z_DEFAULT_STRATEGY)) != Z_OK) {
		pd->state = STATE_WLOADERR;
		return 0;
	}

	stream.avail_in = maplen;
	stream.next_in = mapdata;

	do {
		stream.next_out = out;
		stream.avail_out = 1024;

		if((ret = deflate(&stream, Z_FINISH)) == Z_STREAM_ERROR) {
			pd->state = STATE_WLOADERR;
			goto end;
		}

		*len = htons(1024 - (uint16_t)stream.avail_out);
		if(client->closed || !Client_Send(client, 1028)) {
			pd->state = STATE_WLOADERR;
			goto end;
		}
	} while(stream.avail_out == 0);
	pd->state = STATE_WLOADDONE;

	end:
	deflateEnd(&stream);
	Mutex_Unlock(client->mutex);
	if(pd->state == STATE_WLOADDONE) {
		Packet_WriteLvlFin(client);
		Client_Spawn(client);
	}

	return 0;
}

void Client_Init(void) {
	Client_Broadcast = Memory_Alloc(1, sizeof(struct client));
	Client_Broadcast->wrbuf = Memory_Alloc(2048, 1);
	Client_Broadcast->mutex = Mutex_Create();
}

bool Client_IsInGame(CLIENT client) {
	if(!client->playerData) return false;
	return client->playerData->state == STATE_INGAME;
}

bool Client_IsInSameWorld(CLIENT client, CLIENT other) {
	if(!client->playerData || !other->playerData) return false;
	return client->playerData->world == other->playerData->world;
}

bool Client_IsInWorld(CLIENT client, WORLD world) {
	if(!client->playerData) return false;
	return client->playerData->world == world;
}

bool Client_IsOP(CLIENT client) {
	PLAYERDATA pd = client->playerData;
	return pd ? pd->isOP : false;
}

//TODO: ClassiCube auth
bool Client_CheckAuth(CLIENT client) {
	return Heartbeat_CheckKey(client);
}

void Client_SetPos(CLIENT client, VECTOR* pos, ANGLE* ang) {
	PLAYERDATA pd = client->playerData;
	if(!pd) return;
	Memory_Copy(pd->position, pos, sizeof(struct vector));
	Memory_Copy(pd->angle, ang, sizeof(struct angle));
}

bool Client_SetBlock(CLIENT client, short x, short y, short z, BlockID id) {
	if(client->playerData->state != STATE_INGAME) return false;
	Packet_WriteSetBlock(client, x, y, z, id);
	return true;
}

bool Client_SetProperty(CLIENT client, uint8_t property, int32_t value) {
	if(Client_GetExtVer(client, EXT_MAPASPECT)) {
		CPEPacket_WriteMapProperty(client, property, value);
		return true;
	}
	return false;
}

bool Client_SetTexturePack(CLIENT client, const char* url) {
	if(Client_GetExtVer(client, EXT_MAPASPECT)) {
		CPEPacket_WriteTexturePack(client, url);
		return true;
	}
	return false;
}

bool Client_SetWeather(CLIENT client, Weather type) {
	if(Client_GetExtVer(client, EXT_WEATHER)) {
		CPEPacket_WriteWeatherType(client, type);
		return true;
	}
	return false;
}

bool Client_SetInvOrder(CLIENT client, Order order, BlockID block) {
	if(!Block_IsValid(block)) return false;

	if(Client_GetExtVer(client, EXT_INVORDER)) {
		CPEPacket_WriteInventoryOrder(client, order, block);
		return true;
	}
	return false;
}

bool Client_SetHeld(CLIENT client, BlockID block, bool canChange) {
	if(!Block_IsValid(block)) return false;
	if(Client_GetExtVer(client, EXT_HELDBLOCK)) {
		CPEPacket_WriteHoldThis(client, block, canChange);
		return true;
	}
	return false;
}

bool Client_SetHotbar(CLIENT client, Order pos, BlockID block) {
	if(!Block_IsValid(block) || pos > 8) return false;
	if(Client_GetExtVer(client, EXT_SETHOTBAR)) {
		CPEPacket_WriteSetHotBar(client, pos, block);
		return true;
	}
	return false;
}

bool Client_SetBlockPerm(CLIENT client, BlockID block, bool allowPlace, bool allowDestroy) {
	if(!Block_IsValid(block)) return false;
	if(Client_GetExtVer(client, EXT_BLOCKPERM)) {
		CPEPacket_WriteBlockPerm(client, block, allowPlace, allowDestroy);
		return true;
	}
	return false;
}

bool Client_SetModel(CLIENT client, int16_t model) {
	if(!client->cpeData) return false;
	if(!CPE_CheckModel(model)) return false;
	client->cpeData->model = model;

	for(ClientID i = 0; i < MAX_CLIENTS; i++) {
		CLIENT other = Clients_List[i];
		if(!other || !Client_GetExtVer(other, EXT_CHANGEMODEL)) continue;
		CPEPacket_WriteSetModel(other, other == client ? 0xFF : client->id, model);
	}
	return true;
}

bool Client_SetModelStr(CLIENT client, const char* model) {
	return Client_SetModel(client, CPE_GetModelNum(model));
}

bool Client_SetHacks(CLIENT client) {
	if(Client_GetExtVer(client, EXT_HACKCTRL)) {
		CPEPacket_WriteHackControl(client, client->cpeData->hacks);
		return true;
	}
	return false;
}

static void SocketWaitClose(CLIENT client) {
	Socket_Shutdown(client->sock, SD_SEND);
	while(Socket_Receive(client->sock, client->rdbuf, 131, 0) > 0) {}
	Socket_Close(client->sock);
}

void Client_Free(CLIENT client) {
	if(client->id != 0xFF)
		Clients_List[client->id] = NULL;

	if(client->mutex) Mutex_Free(client->mutex);

	if(client->thread) Thread_Close(client->thread);

	if(client->mapThread) Thread_Close(client->mapThread);

	if(client->websock) Memory_Free(client->websock);

	PLAYERDATA pd = client->playerData;

	if(pd) {
		Memory_Free((void*)pd->name);
		Memory_Free((void*)pd->key);
		Memory_Free(pd->position);
		Memory_Free(pd->angle);
		Memory_Free(pd);
	}

	CPEDATA cpd = client->cpeData;

	if(cpd) {
		EXT prev, ptr = cpd->firstExtension;

		while(ptr) {
			prev = ptr;
			Memory_Free((void*)ptr->name);
			ptr = ptr->next;
			Memory_Free(prev);
		}

		if(cpd->hacks) Memory_Free(cpd->hacks);
		if(cpd->message) Memory_Free(cpd->message);
		if(cpd->appName) Memory_Free((void*)cpd->appName);
		Memory_Free(cpd);
	}

	SocketWaitClose(client);
	Memory_Free(client->rdbuf);
	Memory_Free(client->wrbuf);
	Memory_Free(client);
}

int32_t Client_Send(CLIENT client, int32_t len) {
	if(client->closed) return 0;
	if(client == Client_Broadcast) {
		for(ClientID i = 0; i < MAX_CLIENTS; i++) {
			CLIENT bClient = Clients_List[i];

			if(bClient && !bClient->closed) {
				Mutex_Lock(bClient->mutex);
				if(bClient->websock)
					WsClient_SendHeader(bClient->websock, 0x02, (uint16_t)len);
				Socket_Send(bClient->sock, client->wrbuf, len);
				Mutex_Unlock(bClient->mutex);
			}
		}
		return len;
	}

	if(client->websock)
		WsClient_SendHeader(client->websock, 0x02, (uint16_t)len);
	return Socket_Send(client->sock, client->wrbuf, len);
}

bool Client_Spawn(CLIENT client) {
	if(client->playerData->spawned) return false;
	WORLD world = client->playerData->world;

	Client_SetWeather(client, world->info->wt);
	for(uint8_t prop = 0; prop < WORLD_PROPS_COUNT; prop++) {
		Client_SetProperty(client, prop, World_GetProperty(world, prop));
	}

	for(ClientID i = 0; i < MAX_CLIENTS; i++) {
		CLIENT other = Clients_List[i];
		if(other && Client_IsInSameWorld(client, other)) {
			Packet_WriteSpawn(other, client);

			if(other->cpeData && client->cpeData && Client_GetExtVer(other, EXT_CHANGEMODEL))
				CPEPacket_WriteSetModel(other, other == client ? 0xFF : client->id, Client_GetModel(client));

			if(client != other) {
				Packet_WriteSpawn(client, other);

				if(other->cpeData && client->cpeData && Client_GetExtVer(client, EXT_CHANGEMODEL))
					CPEPacket_WriteSetModel(client, other->id, Client_GetModel(other));
			}
		}
	}

	client->playerData->spawned = true;
	Event_Call(EVT_ONSPAWN, (void*)client);
	return true;
}

bool Client_SendMap(CLIENT client, WORLD world) {
	if(client->mapThread) return false;
	PLAYERDATA pd = client->playerData;
	pd->world = world;
	pd->state = STATE_MOTD;
	Packet_WriteLvlInit(client);
	client->mapThread = Thread_Create(Client_MapThreadProc, client);
	return true;
}

void Client_HandshakeStage2(CLIENT client) {
	Client_ChangeWorld(client, Worlds_List[0]);
}

void Client_Kick(CLIENT client, const char* reason) {
	if(client->closed) return;
	if(!reason) reason = Lang_Get(LANG_KICKNOREASON);
	Packet_WriteKick(client, reason);
	client->closed = true;
	/*
		Этот вызов нужен, чтобы корректно завершить
		сокет клиента после кика, если цикл сервера
		в основом потоке уже не работает.
	*/
	if(!Server_Active) Client_Tick(client);
}

void Client_UpdatePositions(CLIENT client) {
	for(ClientID i = 0; i < MAX_CLIENTS; i++) {
		CLIENT other = Clients_List[i];
		if(other && client != other && Client_IsInGame(other) && Client_IsInSameWorld(client, other))
			Packet_WritePosAndOrient(other, client);
	}
}

void Client_Tick(CLIENT client) {
	PLAYERDATA pd = client->playerData;
	if(client->closed) {
		if(pd && pd->state > STATE_WLOADDONE)
			Event_Call(EVT_ONDISCONNECT, (void*)client);
		Client_Despawn(client);
		Client_Free(client);
		return;
	}

	if(!pd) return;

	if(client->ppstm < 1000) {
		client->ppstm += Server_Delta;
	} else {
		if(client->pps > MAX_CLIENT_PPS) {
			Client_Kick(client, Lang_Get(LANG_KICKPACKETSPAM));
			return;
		}
		client->pps = 0;
		client->ppstm = 0;
	}

	switch (pd->state) {
		case STATE_WLOADDONE:
			pd->state = STATE_INGAME;
			Thread_Close(client->mapThread);
			client->mapThread = NULL;
			break;
		case STATE_WLOADERR:
			Client_Kick(client, Lang_Get(LANG_KICKMAPFAIL));
			break;
	}
}
