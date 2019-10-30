#ifndef PLATFORM_H
#define PLATFORM_H
typedef struct {
  char fmt[256];
  const char* cfile;
  bool isDir;
  char state;
  ITER_DIR dirHandle;
  ITER_FILE fileHandle;
} dirIter;

API void* Memory_Alloc(size_t num, size_t size);
API void* Memory_Realloc(void* buf, size_t old, size_t new);
API void  Memory_Copy(void* dst, const void* src, size_t count);
API void  Memory_Fill(void* dst, size_t count, int32_t val);
API void  Memory_Free(void* ptr);

API bool Iter_Init(dirIter* iter, const char* path, const char* ext);
API bool Iter_Next(dirIter* iter);
API bool Iter_Close(dirIter* iter);

API bool File_Rename(const char* path, const char* newpath);
API FILE* File_Open(const char* path, const char* mode);
API size_t File_Read(void* ptr, size_t size, size_t count, FILE* fp);
API int32_t File_ReadLine(FILE* fp, char* line, int32_t len);
API size_t File_Write(const void* ptr, size_t size, size_t count, FILE* fp);
API int32_t File_GetChar(FILE* fp);
API bool File_Error(FILE* fp);
API bool File_WriteFormat(FILE* fp, const char* fmt, ...);
API bool File_Flush(FILE* fp);
API int32_t File_Seek(FILE* fp, long offset, int32_t origin);
API bool File_Close(FILE* fp);

API bool Directory_Exists(const char* dir);
API bool Directory_Create(const char* dir);
API bool Directory_Ensure(const char* dir);
API bool Directory_SetCurrentDir(const char* path);

bool DLib_Load(const char* path, void** lib);
bool DLib_Unload(void* lib);
char* DLib_GetError(char* buf, size_t len);
bool DLib_GetSym(void* lib, const char* sname, void** sym);

bool Socket_Init(void);
API Socket Socket_New(void);
API int32_t Socket_SetAddr(struct sockaddr_in* ssa, const char* ip, uint16_t port);
API bool Socket_SetAddrGuess(struct sockaddr_in* ssa, const char* host, uint16_t port);
API bool Socket_Bind(Socket sock, struct sockaddr_in* ssa);
API bool Socket_Connect(Socket sock, struct sockaddr_in* ssa);
API Socket Socket_Accept(Socket sock, struct sockaddr_in* addr);
API int32_t Socket_Receive(Socket sock, char* buf, int32_t len, int32_t flags);
API int32_t Socket_ReceiveLine(Socket sock, char* line, int32_t len);
API int32_t Socket_Send(Socket sock, char* buf, int32_t len);
API void Socket_Shutdown(Socket sock, int32_t how);
API void Socket_Close(Socket sock);

API Thread Thread_Create(TFUNC func, const TARG param);
API bool Thread_IsValid(Thread th);
API void Thread_Close(Thread th);
API void Thread_Join(Thread th);

API Mutex* Mutex_Create(void);
API void Mutex_Free(Mutex* handle);
API void Mutex_Lock(Mutex* handle);
API void Mutex_Unlock(Mutex* handle);

API void Time_Format(char* buf, size_t len);
API uint64_t Time_GetMSec(void);

API void Process_Exit(uint32_t ecode);
#endif
