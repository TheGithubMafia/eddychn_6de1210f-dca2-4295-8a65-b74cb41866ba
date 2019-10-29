#ifndef ERROR_H
#define ERROR_H
enum ErrorTypes {
	ET_NOERR = -1,
	ET_SERVER,
	ET_ZLIB,
	ET_SYS,
	ET_STR
};

enum ErrorCodes {
	EC_OK,
	EC_MAGIC,
	EC_FILECORR,
	EC_CFGEND,
	EC_CFGLINEPARSE,
	EC_CFGUNK,
	EC_CFGINVGET,
	EC_ITERINITED,
	EC_INVALIDIP
};

#define Error_Print2(etype, ecode, abort) \
Error_Print(etype, ecode, __FILE__, __LINE__, __func__); \
if(abort) { \
	Process_Exit(ecode); \
}
#define Error_PrintF2(etype, ecode, abort, ...) \
Error_PrintF(etype, ecode, __FILE__, __LINE__, __func__, __VA_ARGS__); \
if(abort) { \
	Process_Exit(ecode); \
}
#if defined(WINDOWS)
#  define Error_PrintSys(abort) Error_Print2(ET_SYS, GetLastError(), abort);
#elif defined(POSIX)
#  define Error_PrintSys(abort) Error_Print2(ET_SYS, errno, abort);
#endif

API int Error_GetSysCode(void);
API void Error_Print(int type, uint32_t code, const char* file, uint32_t line, const char* func);
API void Error_PrintF(int type, uint32_t code, const char* file, uint32_t line, const char* func, ...);
#endif
