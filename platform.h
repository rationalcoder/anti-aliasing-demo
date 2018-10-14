#pragma once

#ifdef _WIN64
    #include "win32_tanks.h"
#else
    #error "Unsupported platform"
#endif

#include <stdint.h>
#include <stdarg.h>

#include "globals.h"

// NOTE(blake): the standard ones confuse Qt Creator.
#define MAX_UINT(type) ~(type)0

// NOTE(blake): the use of char here is mostly a joke; apparently, the int8 types
// don't have to be chars, making certain casting business technically UB...

using u8  = unsigned char;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using umm = size_t;

using uptr = uintptr_t;

using s8  = char;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using b32 = s32;

// TODO(blake): fast ints, least ints, etc.

using f32 = float;
using f64 = double;

enum Log_Level
{
    LogLevel_Debug,
    LogLevel_Info,
    LogLevel_Warn,
    LogLevel_Critical,
    LogLevel_Fatal,
};

#define log_debug(fmtLiteral, ...) log_printf_(LogLevel_Debug,    "[DEBUG] "    __FILE__ ":%d: " fmtLiteral, __LINE__, __VA_ARGS__)
#define log_info(fmtLiteral, ...)  log_printf_(LogLevel_Info,     "[INFO] "     __FILE__ ":%d: " fmtLiteral, __LINE__, __VA_ARGS__)
#define log_warn(fmtLiteral, ...)  log_printf_(LogLevel_Warn,     "[WARNING] "  __FILE__ ":%d: " fmtLiteral, __LINE__, __VA_ARGS__)
#define log_crit(fmtLiteral, ...)  log_printf_(LogLevel_Critical, "[CRITICAL] " __FILE__ ":%d: " fmtLiteral, __LINE__, __VA_ARGS__)
#define log_fatal(fmtLiteral, ...) log_printf_(LogLevel_Fatal,    "[FATAL] "    __FILE__ ":%d: " fmtLiteral, __LINE__, __VA_ARGS__)

extern void
log_printf_(Log_Level level, const char* fmt, ...);


#define PLATFORM_EXPAND_ARENA(name) b32 name(struct Memory_Arena* arena, umm size)
typedef PLATFORM_EXPAND_ARENA(Platform_Expand_Arena);

#define PLATFORM_READ_ENTIRE_FILE(name) void* name(const char* name, struct Memory_Arena* arena, umm* size, u32 alignment)
typedef PLATFORM_READ_ENTIRE_FILE(Platform_Read_Entire_File);

// len == -1 to assume nul-terminated.
#define PLATFORM_LOG(name) void name(Log_Level level, const char* str, s32 len)
typedef PLATFORM_LOG(Platform_Log);

struct Platform
{
    Platform_Expand_Arena* expand_arena = nullptr;
    Platform_Expand_Arena* failed_expand_arena = nullptr;

    Platform_Read_Entire_File* read_entire_file = nullptr;
    Platform_Log* log = nullptr;

    b32 initialized = false; // useful for asserts
};


