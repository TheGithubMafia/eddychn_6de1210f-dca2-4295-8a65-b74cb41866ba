#include "core.h"
#include "error.h"
#include <string.h>

#ifdef _WIN32
/*
	WINDOWS SOCKET FUNCTIONS
*/
bool Socket_Init() {
	WSADATA ws;
	if(WSAStartup(MAKEWORD(1, 1), &ws) == SOCKET_ERROR) {
		Error_Set(ET_SYS, WSAGetLastError());
		return false;
	}
	return true;
}

SOCKET Socket_Bind(const char* ip, ushort port) {
	SOCKET fd;

	if(INVALID_SOCKET == (fd = socket(AF_INET, SOCK_STREAM, 0))) {
		Error_Set(ET_SYS, WSAGetLastError());
		return INVALID_SOCKET;
	}

	struct sockaddr_in ssa;
	ssa.sin_family = AF_INET;
	ssa.sin_port = htons(port);
	if(inet_pton(AF_INET, ip, &ssa.sin_addr.s_addr) <= 0) {
		Error_Set(ET_SYS, WSAGetLastError());
		return INVALID_SOCKET;
	}

	if(bind(fd, (const struct sockaddr*)&ssa, sizeof ssa) == -1) {
		Error_Set(ET_SYS, WSAGetLastError());
		return INVALID_SOCKET;
	}

	if(listen(fd, SOMAXCONN) == -1) {
		Error_Set(ET_SYS, WSAGetLastError());
		return INVALID_SOCKET;
	}

	return fd;
}

void Socket_Close(SOCKET sock) {
	closesocket(sock);
}

/*
	WINDOWS THREAD FUNCTIONS
*/
THREAD Thread_Create(TFUNC func, TARG lpParam) {
	return CreateThread(
		NULL,
		0,
		func,
		lpParam,
		0,
		NULL
	);
}

void Thread_Close(THREAD th) {
	if(th)
		CloseHandle(th);
}

/*
	WINDOWS STRING FUNCTIONS
*/
bool String_CaselessCompare(const char* str1, const char* str2) {
	return _stricmp(str1, str2) == 0;
}

bool String_Compare(const char* str1, const char* str2) {
	return strcmp(str1, str2) == 0;
}

size_t String_Length(const char* str) {
	return strlen(str);
}

bool String_Copy(char* dst, size_t len, const char* src) {
	if(!dst) return false;
	if(!src) return false;

	if(String_Length(src) < len)
		strcpy(dst, src);
	else
		return false;

	return true;
}

char* String_CopyUnsafe(char* dst, const char* src) {
	return strcpy(dst, src);
}

uint String_FormatError(uint code, char* buf, uint buflen) {
	uint len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, 0, buf, buflen, NULL);
	if(len > 0) {
		Error_WinBuf[len - 1] = 0;
		Error_WinBuf[len - 2] = 0;
	} else {
		Error_Set(ET_SYS, GetLastError());
	}

	return len;
}

/*
	WINDOWS TIME FUNCTIONS
*/
void Time_Format(char* buf, size_t buflen) {
	SYSTEMTIME time;
	GetSystemTime(&time);
	sprintf_s(buf, buflen, "%02d:%02d:%02d.%03d",
		time.wHour,
		time.wMinute,
		time.wSecond,
		time.wMilliseconds
	);
}
#else
#include <pthread.h>

/*
	POSIX SOCKET FUNCTIONS
*/
bool Socket_Init() {
	return true;
}

SOCKET Socket_Bind(const char*, ushort port) {
	SOCKET fd;

	if((fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		Error_Set(ET_SYS, GetLastError());
		return INVALID_SOCKET;
	}
}

void Socket_Close(SOCKET fd) {
	close(fd);
}

/*
	POSIX THREAD FUNCTIONS
*/
THREAD Thread_Create(TFUNC func, TARG arg) {
	return NULL;
}

void Thread_Close(THREAD th) {

}

/*
	POSIX STRING FUNCTIONS
*/
bool String_CaselessCompare(const char* str1, const char* str2) {
	return stricmp(str1, str2) == 0;
}

bool String_Compare(const char* str1, const char* str2) {
	return strcmp(str1, str2) == 0;
}

size_t String_Length(const char* str) {
	return strlen(str);
}

uint String_Copy(char* dst, size_t len, const char* src) {
	if(!dst) return false;
	if(!src) return false;

	if(String_Length(src) < len)
		strcpy(dst, src);
	else
		return false;

	return true;
}

char* String_CopyUnsafe(char* dst, const char* src) {
	return strcpy(dst, src);
}

uint String_FormatError(uint code, char* buf, uint buflen) {

}

/*
	POSIX TIME FUNCTIONS
*/

void Time_Format(char* buf, size_t buflen) {
	if(buflen > 12)
		String_Copy(buf, "00:00:00.000");
}
#endif
