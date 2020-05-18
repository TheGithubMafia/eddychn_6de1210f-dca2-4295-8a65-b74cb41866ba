#include "core.h"
#include "platform.h"
#include "str.h"
#include "error.h"
#include <stdio.h>

#if defined(WINDOWS)
HANDLE hHeap;

void Memory_Init(void) {
	hHeap = HeapCreate(
		HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE,
		0x01000, 0x00000
	);
}

void Memory_Uninit(void) {
	HeapDestroy(hHeap);
}

void *Memory_Alloc(cs_size num, cs_size size) {
	return HeapAlloc(hHeap, HEAP_ZERO_MEMORY, num * size);
}

void *Memory_Realloc(void *buf, cs_size old, cs_size new) {
	(void)old;
	return HeapReAlloc(hHeap, HEAP_ZERO_MEMORY, buf, new);
}

void Memory_Free(void *ptr) {
	HeapFree(hHeap, 0, ptr);
}
#elif defined(POSIX)
#include <stdlib.h>

void Memory_Init(void) {}
void Memory_Uninit(void) {}

void *Memory_Alloc(cs_size num, cs_size size) {
	void *ptr;
	if((ptr = calloc(num, size)) == NULL) {
		Error_PrintSys(true)
	}
	return ptr;
}

void *Memory_Realloc(void *buf, cs_size old, cs_size new) {
	cs_char *pNew = realloc(buf, new);
	if(new > old)
		Memory_Zero(pNew + old, new - old);
	return pNew;
}

void Memory_Free(void *ptr) {
	free(ptr);
}
#endif

void Memory_Copy(void *dst, const void *src, cs_size count) {
	cs_byte *u8dst = (cs_byte *)dst;
	cs_byte *u8src = (cs_byte *)src;

	while(count > 0) {
		*u8dst++ = *u8src++;
		count--;
	}
}

void Memory_Fill(void *dst, cs_size count, cs_byte val) {
	cs_byte *u8dst = (cs_byte *)dst;

	while(count > 0) {
		*u8dst++ = val;
		count--;
	}
}

cs_bool File_Rename(cs_str path, cs_str newpath) {
#if defined(WINDOWS)
	return (cs_bool)MoveFileExA(path, newpath, MOVEFILE_REPLACE_EXISTING);
#elif defined(POSIX)
	return rename(path, newpath) == 0;
#endif
}

FILE *File_Open(cs_str path, cs_str mode) {
	return fopen(path, mode);
}

cs_size File_Read(void *ptr, cs_size size, cs_size count, FILE *fp) {
	return fread(ptr, size, count, fp);
}

cs_int32 File_ReadLine(FILE *fp, cs_char *line, cs_int32 len) {
	cs_int32 bleft = len;

	while(bleft > 1) {
		cs_int32 ch = File_GetChar(fp);
		if(ch == '\n' || ch == EOF) break;
		if(ch != '\r') {
			*line++ = (char)ch;
			bleft--;
		}
	}

	*line = '\0';
	return len - bleft;
}

cs_size File_Write(const void *ptr, cs_size size, cs_size count, FILE *fp) {
	return fwrite(ptr, size, count, fp);
}

cs_int32 File_GetChar(FILE *fp) {
	return fgetc(fp);
}

cs_bool File_Error(FILE *fp) {
	return ferror(fp) != 0;
}

cs_bool File_WriteFormat(FILE *fp, cs_str fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(fp, fmt, args);
	va_end(args);

	return !File_Error(fp);
}

cs_bool File_Flush(FILE *fp) {
	return fflush(fp) == 0;
}

cs_int32 File_Seek(FILE *fp, long offset, cs_int32 origin) {
	return fseek(fp, offset, origin);
}

cs_bool File_Close(FILE *fp) {
	return fclose(fp) != 0;
}

cs_bool Socket_Init(void) {
#if defined(WINDOWS)
	WSADATA ws;
	return WSAStartup(MAKEWORD(1, 1), &ws) != SOCKET_ERROR;
#else
	return true;
#endif
}

cs_int32 Socket_SetAddr(struct sockaddr_in *ssa, cs_str ip, cs_uint16 port) {
	ssa->sin_family = AF_INET;
	ssa->sin_port = htons(port);
	return inet_pton(AF_INET, ip, &ssa->sin_addr.s_addr);
}

cs_bool Socket_SetAddrGuess(struct sockaddr_in *ssa, cs_str host, cs_uint16 port) {
	cs_int32 ret;
	if((ret = Socket_SetAddr(ssa, host, port)) == 0) {
		struct addrinfo *addr;
		struct addrinfo hints = {0};
		hints.ai_family = AF_INET;
		hints.ai_socktype = 0;
		hints.ai_protocol = 0;

		cs_char strport[6];
		String_FormatBuf(strport, 6, "%d", port);
		if((ret = getaddrinfo(host, strport, &hints, &addr)) == 0) {
			*ssa = *(struct sockaddr_in *)addr->ai_addr;
			freeaddrinfo(addr);
			return true;
		}
	}
	return ret == 1;
}

Socket Socket_New() {
	return socket(AF_INET, SOCK_STREAM, 0);
}

cs_bool Socket_Bind(Socket sock, struct sockaddr_in *addr) {
#if defined(POSIX)
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(cs_int32){1}, 4) == -1) {
		return false;
	}
#endif

	if(bind(sock, (const struct sockaddr *)addr, sizeof(struct sockaddr_in)) == -1) {
		return false;
	}

	if(listen(sock, SOMAXCONN) == -1) {
		return false;
	}

	return true;
}

cs_bool Socket_Connect(Socket sock, struct sockaddr_in *addr) {
	socklen_t len = sizeof(struct sockaddr_in);
	return connect(sock, (struct sockaddr *)addr, len) == 0;
}

Socket Socket_Accept(Socket sock, struct sockaddr_in *addr) {
	socklen_t len = sizeof(struct sockaddr_in);
	return accept(sock, (struct sockaddr *)addr, &len);
}

#if defined(WINDOWS)
#define SOCK_DFLAGS 0
#elif defined(POSIX)
#define SOCK_DFLAGS MSG_NOSIGNAL
#endif

cs_int32 Socket_Receive(Socket sock, cs_char *buf, cs_int32 len, cs_int32 flags) {
	return recv(sock, buf, len, SOCK_DFLAGS | flags);
}

cs_int32 Socket_ReceiveLine(Socket sock, cs_char *line, cs_int32 len) {
	cs_int32 start_len = len;
	cs_char sym;

	while(len > 1) {
		if(Socket_Receive(sock, &sym, 1, MSG_WAITALL) == 1) {
			if(sym == '\n') {
				*line++ = '\0';
				break;
			} else if(sym != '\r') {
				*line++ = sym;
				--len;
			}
		} else return 0;
	}

	*line = '\0';
	return start_len - len;
}

cs_int32 Socket_Send(Socket sock, const cs_char *buf, cs_int32 len) {
	return send(sock, buf, len, 0);
}

void Socket_Shutdown(Socket sock, cs_int32 how) {
	shutdown(sock, how);
}

void Socket_Close(Socket sock) {
#if defined(WINDOWS)
	closesocket(sock);
#elif defined(POSIX)
	close(sock);
#endif
}

#if defined(WINDOWS)
#define ISDIR(h) (h.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
cs_bool Iter_Init(DirIter *iter, cs_str dir, cs_str ext) {
	iter->state = ITER_INITIAL;
	iter->dirHandle = INVALID_HANDLE_VALUE;

	String_FormatBuf(iter->fmt, 256, "%s\\*.%s", dir, ext);
	if((iter->dirHandle = FindFirstFileA(iter->fmt, &iter->fileHandle)) == INVALID_HANDLE_VALUE) {
		if(GetLastError() == ERROR_FILE_NOT_FOUND)
			iter->state = ITER_DONE;
		else
			iter->state = ITER_ERROR;
		return false;
	}

	iter->cfile = iter->fileHandle.cFileName;
	iter->isDir = ISDIR(iter->fileHandle);
	iter->state = ITER_READY;
	return true;
}

cs_bool Iter_Next(DirIter *iter) {
	if(iter->state != ITER_READY)
		return false;

	if(FindNextFile(iter->dirHandle, &iter->fileHandle)) {
		iter->isDir = ISDIR(iter->fileHandle);
		iter->cfile = iter->fileHandle.cFileName;
		return true;
	}

	iter->state = ITER_DONE;
	return false;
}

cs_bool Iter_Close(DirIter *iter) {
	if(iter->state == ITER_INITIAL)
		return false;
	FindClose(iter->dirHandle);
	return true;
}
#elif defined(POSIX)
static cs_bool checkExtension(cs_str filename, cs_str ext) {
	cs_str _ext = String_LastChar(filename, '.');
	if(!_ext && !ext) return true;
	if(!_ext || !ext) return false;

	return String_Compare(++_ext, ext);
}

cs_bool Iter_Init(DirIter *iter, cs_str dir, cs_str ext) {
	iter->state = ITER_INITIAL;
	iter->fileHandle = NULL;
	iter->dirHandle = opendir(dir);
	if(!iter->dirHandle) {
		iter->state = ITER_ERROR;
		return false;
	}

	String_Copy(iter->fmt, 256, ext);
	iter->state = ITER_READY;
	return Iter_Next(iter);
}

cs_bool Iter_Next(DirIter *iter) {
	if(iter->state != ITER_READY)
		return false;

	do {
		if((iter->fileHandle = readdir(iter->dirHandle)) == NULL) {
			iter->state = ITER_DONE;
			return false;
		} else {
			iter->cfile = iter->fileHandle->d_name;
			iter->isDir = iter->fileHandle->d_type == DT_DIR;
		}
	} while(!iter->cfile || !checkExtension(iter->cfile, iter->fmt));

	return true;
}

cs_bool Iter_Close(DirIter *iter) {
	if(iter->state == ITER_INITIAL)
		return false;
	if(iter->dirHandle)
		closedir(iter->dirHandle);
	return true;
}
#endif

#if defined(WINDOWS)
cs_bool Directory_Exists(cs_str path) {
	cs_uint32 attr = GetFileAttributesA(path);
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

cs_bool Directory_SetCurrentDir(cs_str path) {
	return (cs_bool)SetCurrentDirectoryA(path);
}

cs_bool Directory_Create(cs_str path) {
	return (cs_bool)CreateDirectoryA(path, NULL);
}
#elif defined(POSIX)
cs_bool Directory_Exists(cs_str path) {
	struct stat ss;
	return stat(path, &ss) == 0 && S_ISDIR(ss.st_mode);
}

cs_bool Directory_SetCurrentDir(cs_str path) {
	return chdir(path) == 0;
}

cs_bool Directory_Create(cs_str path) {
	return mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
}
#endif

cs_bool Directory_Ensure(cs_str path) {
	if(Directory_Exists(path)) return true;
	return Directory_Create(path);
}

#if defined(WINDOWS)
cs_bool DLib_Load(cs_str path, void **lib) {
	return (*lib = LoadLibraryA(path)) != NULL;
}

cs_bool DLib_Unload(void *lib) {
	return (cs_bool)FreeLibrary(lib);
}

cs_char *DLib_GetError(cs_char *buf, cs_size len) {
	String_FormatError(GetLastError(), buf, len, NULL);
	return buf;
}

cs_bool DLib_GetSym(void *lib, cs_str sname, void *sym) {
	return (*(void **)sym = (void *)GetProcAddress(lib, sname)) != NULL;
}
#elif defined(POSIX)
cs_bool DLib_Load(cs_str path, void **lib) {
	return (*lib = dlopen(path, RTLD_NOW)) != NULL;
}

cs_bool DLib_Unload(void *lib) {
	return dlclose(lib) == 0;
}

cs_char *DLib_GetError(cs_char *buf, cs_size len) {
	String_Copy(buf, len, dlerror());
	return buf;
}

cs_bool DLib_GetSym(void *lib, cs_str sname, void *sym) {
	return (*(void **)sym = dlsym(lib, sname)) != NULL;
}
#endif

#if defined(WINDOWS)
Thread Thread_Create(TFUNC func, TARG param, cs_bool detach) {
	Thread th = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)func,
		param,
		0,
		NULL
	);

	if(!th) {
		ERROR_PRINT(ET_SYS, GetLastError(), true)
	}

	if(detach) {
		Thread_Detach(th);
		th = NULL;
	}
	return th;
}

cs_bool Thread_IsValid(Thread th) {
	return th != (Thread)NULL;
}

void Thread_Detach(Thread th) {
	if(!CloseHandle(th)) {
		Error_PrintSys(true)
	}
}

void Thread_Join(Thread th) {
	WaitForSingleObject(th, INFINITE);
	Thread_Detach(th);
}
#elif defined(POSIX)
Thread Thread_Create(TFUNC func, TARG arg, cs_bool detach) {
	Thread th = Memory_Alloc(1, sizeof(Thread));
	if(pthread_create(th, NULL, func, arg) != 0) {
		ERROR_PRINT(ET_SYS, errno, true)
		return NULL;
	}

	if(detach) Thread_Detach(th);
	return th;
}

cs_bool Thread_IsValid(Thread th) {
	return th != NULL;
}

void Thread_Detach(Thread th) {
	pthread_detach(*th);
	Memory_Free(th);
}

void Thread_Join(Thread th) {
	cs_int32 ret = pthread_join(*th, NULL);
	if(ret) {
		ERROR_PRINT(ET_SYS, ret, true)
	}
	Memory_Free(th);
}
#endif

#if defined(WINDOWS)
Mutex *Mutex_Create(void) {
	Mutex *ptr = Memory_Alloc(1, sizeof(Mutex));
	if(!ptr) {
		ERROR_PRINT(ET_SYS, GetLastError(), true)
	}
	InitializeCriticalSection(ptr);
	return ptr;
}

void Mutex_Free(Mutex *handle) {
	DeleteCriticalSection(handle);
	Memory_Free(handle);
}

void Mutex_Lock(Mutex *handle) {
	EnterCriticalSection(handle);
}

void Mutex_Unlock(Mutex *handle) {
	LeaveCriticalSection(handle);
}

Waitable *Waitable_Create(void) {
	Waitable *handle = CreateEventA(NULL, true, false, NULL);
	if(!handle) {
		Error_PrintSys(true)
	}
	return handle;
}

void Waitable_Free(Waitable *handle) {
	if(!CloseHandle(handle)) {
		Error_PrintSys(true)
	}
}

void Waitable_Reset(Waitable *handle) {
	ResetEvent(handle);
}

void Waitable_Signal(Waitable *handle) {
	SetEvent(handle);
}

void Waitable_Wait(Waitable *handle) {
	WaitForSingleObject(handle, INFINITE);
}
#elif defined(POSIX)
#include <fcntl.h>
#include <poll.h>

Mutex *Mutex_Create(void) {
	Mutex *ptr = Memory_Alloc(1, sizeof(Mutex));
	cs_int32 ret = pthread_mutex_init(ptr, NULL);
	if(ret) {
		ERROR_PRINT(ET_SYS, ret, true)
		return NULL;
	}
	return ptr;
}

void Mutex_Free(Mutex *handle) {
	cs_int32 ret = pthread_mutex_destroy(handle);
	if(ret) {
		ERROR_PRINT(ET_SYS, ret, true)
	}
	Memory_Free(handle);
}

void Mutex_Lock(Mutex *handle) {
	cs_int32 ret = pthread_mutex_lock(handle);
	if(ret) {
		ERROR_PRINT(ET_SYS, ret, true)
	}
}

void Mutex_Unlock(Mutex *handle) {
	cs_int32 ret = pthread_mutex_unlock(handle);
	if(ret) {
		ERROR_PRINT(ET_SYS, ret, true)
	}
}

Waitable *Waitable_Create(void) {
	Waitable *handle = Memory_Alloc(1, sizeof(Waitable));
	if(pipe(handle->pipefd) == 0) {
		if(fcntl(handle->pipefd[0], F_SETFL, O_NONBLOCK) != 0) {
			Waitable_Free(handle);
			return NULL;
		}
		return handle;
	}
	return NULL;
}

void Waitable_Free(Waitable *handle) {
	close(handle->pipefd[0]);
	close(handle->pipefd[1]);
	Memory_Free(handle);
}

void Waitable_Signal(Waitable *handle) {
	write(handle->pipefd[1], &handle->buf, 1);
}

void Waitable_Reset(Waitable *handle) {
	read(handle->pipefd[0], &handle->buf, 1);
}

void Waitable_Wait(Waitable *handle) {
	struct pollfd pfd;
	pfd.fd = handle->pipefd[0];
	pfd.events = POLLRDNORM;
	poll(&pfd, 1, -1);
}
#endif

#if defined(WINDOWS)
void Time_Format(cs_char *buf, cs_size buflen) {
	SYSTEMTIME time;
	GetSystemTime(&time);
	sprintf_s(buf, buflen, "%02d:%02d:%02d.%03d",
		time.wHour,
		time.wMinute,
		time.wSecond,
		time.wMilliseconds
	);
}

cs_uint64 Time_GetMSec(void) {
	FILETIME ft; GetSystemTimeAsFileTime(&ft);
	cs_uint64 time = ft.dwLowDateTime | ((cs_uint64)ft.dwHighDateTime << 32);
	return (time / 10000) + 50491123200000ULL;
}
#elif defined(POSIX)
void Time_Format(cs_char *buf, cs_size buflen) {
	struct timeval tv;
	struct tm *tm;
	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);

	if(buflen > 12) {
		sprintf(buf, "%02d:%02d:%02d.%03d",
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec,
			(cs_int32) (tv.tv_usec / 1000)
		);
	}
}

cs_uint64 Time_GetMSec() {
	struct timeval cur; gettimeofday(&cur, NULL);
	return (cs_uint64)cur.tv_sec * 1000 + 62135596800000ULL + (cur.tv_usec / 1000);
}
#endif

void Process_Exit(cs_int32 code) {
#if defined(WINDOWS)
	ExitProcess(code);
#elif defined(POSIX)
	exit(code);
#endif
}
