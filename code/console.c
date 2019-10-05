#include "core.h"
#include "console.h"
#include "command.h"

THREAD conThread;

int Console_ReadLine(char* buf, int buflen) {
	int len = 0;
	int c = 0;

	while((c = getc(stdin)) != EOF && c != '\n') {
		if(c != '\r') {
			buf[len] = (char)c;
			len++;
			if(len > buflen) {
				len--;
				break;
			}
		}
	}

	buf[len] = 0;
	return len;
}

void Console_HandleCommand(char* cmd) {
	if(!Command_Handle(cmd, NULL))
		Log_Info("Unknown command");
}

TRET Console_ThreadProc(TARG lpParam) {
	Thread_SetName("Console listener");
	char buf[4096] = {0};

	while(1) {
		if(Console_ReadLine(buf, 4096) > 0)
			Console_HandleCommand(buf);
	}
	return 0;
}

void Console_StartListen(void) {
	conThread = Thread_Create(Console_ThreadProc, NULL);
}

void Console_Close(void) {
	if(conThread) Thread_Close(conThread);
}
