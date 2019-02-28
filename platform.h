
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

// len == -1 to assume nul-terminated.
#define PLATFORM_LOG(name_) void name_(Log_Level level, const char* str, s32 len)
typedef PLATFORM_LOG(Platform_Log);

#define PLATFORM_EXPAND_ARENA(name_) b32 name_(struct Memory_Arena* arena, umm size)
typedef PLATFORM_EXPAND_ARENA(Platform_Expand_Arena);

#define PLATFORM_READ_ENTIRE_FILE(name_) void* name_(const char* name, struct Memory_Arena* arena, umm* size, u32 alignment)
typedef PLATFORM_READ_ENTIRE_FILE(Platform_Read_Entire_File);

#define PLATFORM_WRITE_FILE(name_) b32 name_(const char* name, void* data, umm size)
typedef PLATFORM_WRITE_FILE(Platform_Write_File);

#define PLATFORM_TOGGLE_FULLSCREEN(name_) b32 name_()
typedef PLATFORM_TOGGLE_FULLSCREEN(Platform_Toggle_Fullscreen);

#define PLATFORM_ENABLE_VSYNC(name_) void name_(bool enabled)
typedef PLATFORM_ENABLE_VSYNC(Platform_Enable_Vsync);

struct Platform
{
    Platform_Log* log = nullptr;

    Platform_Expand_Arena* expand_arena        = nullptr;
    Platform_Expand_Arena* failed_expand_arena = nullptr;

    Platform_Read_Entire_File* read_entire_file = nullptr;
    Platform_Write_File*       write_file       = nullptr;

    Platform_Toggle_Fullscreen* toggle_fullscreen = nullptr;
    Platform_Enable_Vsync*      enable_vsync      = nullptr;

    b32 initialized = false; // useful for asserts
};

inline PLATFORM_LOG(platform_log) { return gPlatform->log(level, str, len); }
inline PLATFORM_EXPAND_ARENA(platform_expand_arena)  { return gPlatform->expand_arena(arena, size); }
inline PLATFORM_READ_ENTIRE_FILE(platform_read_entire_file)  { return gPlatform->read_entire_file(name, arena, size, alignment); }
inline PLATFORM_WRITE_FILE(platform_write_file) { return gPlatform->write_file(name, data, size); }
inline PLATFORM_TOGGLE_FULLSCREEN(platform_toggle_fullscreen) { return gPlatform->toggle_fullscreen(); }
inline PLATFORM_ENABLE_VSYNC(platform_enable_vsync) { gPlatform->enable_vsync(enabled); }

