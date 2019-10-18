#include "core.h"
#include "server.h"
#include "client.h"
#include "packets.h"
#include "config.h"
#include "console.h"
#include "command.h"
#include "websocket.h"
#include "generators.h"
#include "event.h"
#include "cplugin.h"

static void AcceptFunc(void) {
	struct sockaddr_in caddr;
	SOCKET fd = Socket_Accept(Server_Socket, &caddr);

	if(fd != INVALID_SOCKET) {
		if(!Server_Active) {
			Socket_Close(fd);
			return;
		}
	 	CLIENT tmp = Memory_Alloc(1, sizeof(struct client));

		tmp->id = 0xFF;
		tmp->sock = fd;
		tmp->mutex = Mutex_Create();
		tmp->addr = ntohl(caddr.sin_addr.s_addr);
		tmp->rdbuf = Memory_Alloc(134, 1);
		tmp->wrbuf = Memory_Alloc(2048, 1);

		if(Socket_Receive(fd, tmp->rdbuf, 3, MSG_PEEK)) {
			if(String_CaselessCompare(tmp->rdbuf, "GET")) {
				WSCLIENT wscl = Memory_Alloc(1, sizeof(struct wsClient));
				wscl->recvbuf = tmp->rdbuf;
				wscl->sock = fd;
				tmp->websock = wscl;
				if(!WsClient_DoHandshake(wscl)) {
					Client_Free(tmp);
					Socket_Close(fd);
					return;
				}
			}
		}

		if(Client_Add(tmp))
			tmp->thread = Thread_Create(Client_ThreadProc, tmp);
		else
			Client_Kick(tmp, "Server is full");
	}
}

static TRET AcceptThreadProc(TARG param) {
	(void)param;
	while(Server_Active) AcceptFunc();
	return 0;
}

static bool Bind(const char* ip, uint16_t port) {
	if((Server_Socket = Socket_Bind(ip, port)) == INVALID_SOCKET)
		return false;

	Client_Init();
	Log_Info("%s %s started on %s:%d", SOFTWARE_NAME, SOFTWARE_VERSION, ip, port);
	return true;
}

static void onConnect(void* param) {
	CLIENT cl = (CLIENT)param;
	const char* name = Client_GetName(cl);
	const char* appname = Client_GetAppName(cl);
	Log_Info("Player %s connected with %s", name, appname);
}

static void onDisconnect(void* param) {
	if(!Server_Active) return;
	const char* name = Client_GetName((CLIENT)param);
	Log_Info("Player %s disconnected", name);
}

static bool InitialWork(void) {
	if(!Socket_Init())
		return false;

	Log_Info("Loading " MAINCFG);
	Server_Config = Config_Create(MAINCFG);
	Config_SetStr(Server_Config, "ip", "0.0.0.0");
	Config_AddComment(Server_Config, "Bind server to specified IP address. \"0.0.0.0\" - means \"all available network adapters\".");
	Config_SetInt(Server_Config, "port", 25565);
	Config_AddComment(Server_Config, "Use specified port to accept clients.");
	Config_SetStr(Server_Config, "name", DEFAULT_NAME);
	Config_AddComment(Server_Config, "Server name and MOTD will be shown to the player during map loading.");
	Config_SetStr(Server_Config, "motd", DEFAULT_MOTD);
	Config_SetStr(Server_Config, "loglevel", "EICW");
	Config_AddComment(Server_Config, "E - Errors, I - Info, C - Chat, W - Warnings, D - Debug.");
	Config_SetBool(Server_Config, "alwayslocalop", false);
	Config_AddComment(Server_Config, "Any player with ip address \"127.0.0.1\" will automatically become an operator.");
	if(!Config_Load(Server_Config)) Process_Exit(1);

	Log_SetLevelStr(Config_GetStr(Server_Config, "loglevel"));
	Packet_RegisterDefault();
	Packet_RegisterCPEDefault();
	Command_RegisterDefault();
	Event_RegisterVoid(EVT_ONHANDSHAKEDONE, onConnect);
	Event_RegisterVoid(EVT_ONDISCONNECT, onDisconnect);

	Directory_Ensure("worlds");
	int wIndex = -1;
	dirIter wIter = {0};
	if(Iter_Init(&wIter, "worlds", "cws")) {
		do {
			if(wIter.isDir || !wIter.cfile) continue;
			WORLD tmp = World_Create(wIter.cfile);
			tmp->id = ++wIndex;
			if(!World_Load(tmp) || !World_Add(tmp))
				World_Free(tmp);
		} while(Iter_Next(&wIter) && wIndex < MAX_WORLDS);
	}
	Iter_Close(&wIter);

	if(wIndex < 0) {
		WORLD tmp = World_Create("world.cws");
		World_SetDimensions(tmp, 256, 256, 256);
		World_AllocBlockArray(tmp);
		Generator_Flat(tmp);
		Worlds_List[0] = tmp;
	}

	Log_Info("Loading C plugins");
	CPlugin_Start();

	Event_Call(EVT_POSTSTART, NULL);
	return Bind(Config_GetStr(Server_Config, "ip"), (uint16_t)Config_GetInt(Server_Config, "port"));
}

static void DoStep(void) {
	Event_Call(EVT_ONTICK, NULL);
	for(int i = 0; i < max(MAX_WORLDS, MAX_CLIENTS); i++) {
		CLIENT client = Client_GetByID((ClientID)i);
		WORLD world = World_GetByID(i);

		if(i < MAX_CLIENTS && client) Client_Tick(client);
		if(i < MAX_WORLDS && world) World_Tick(world);
	}
}

static void Stop(void) {
	Event_Call(EVT_ONSTOP, NULL);
	Log_Info("Saving worlds");
	for(int i = 0; i < max(MAX_WORLDS, MAX_CLIENTS); i++) {
		CLIENT client = Client_GetByID((ClientID)i);
		WORLD world = World_GetByID(i);

		if(i < MAX_CLIENTS && client)
			Client_Kick(client, "Server stopped");

		if(i < MAX_WORLDS && world) {
			if(World_Save(world))
				Thread_Join(world->thread);
			World_Free(world);
		}
	}

	Console_Close();
	if(Server_AcceptThread)
		Thread_Close(Server_AcceptThread);

	Socket_Close(Server_Socket);
	Log_Info("Saving " MAINCFG);
	Config_Save(Server_Config);
	Config_DestroyStore(Server_Config);

	Log_Info("Unloading plugins");
	CPlugin_Stop();
}

int main(int argc, char** argv) {
	if(argc < 2 || !String_CaselessCompare(argv[1], "nochdir")) {
		const char* path = String_AllocCopy(argv[0]);
		char* lastSlash = (char*)String_LastChar(path, PATH_DELIM);
		if(lastSlash) {
			*lastSlash = '\0';
			Directory_SetCurrentDir(path);
		}
		Memory_Free((char*)path);
	}

	if((Server_Active = InitialWork()) == true) {
		Console_StartListen();
		Server_StartTime = Time_GetMSec();
		Server_AcceptThread = Thread_Create(AcceptThreadProc, NULL);
	}

	uint64_t curr = Time_GetMSec(), last = 0;
	while(Server_Active) {
		last = curr;
		curr = Time_GetMSec();
		Server_Delta = (uint16_t)(curr - last);
		if(Server_Delta > 500) {
			Log_Warn("Last server tick took %dms!", Server_Delta);
			Server_Delta = 500;
		}
		DoStep();
		Sleep(10);
	}

	Log_Info("Main loop done");
	Stop();
	return 0;
}
