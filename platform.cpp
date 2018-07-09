#include "platform.h"
#include "tanks.h"

static char*
push_stbsp_temp_storage(char* /*str*/, void* /*userData*/, int len)
{
    if (len < STB_SPRINTF_MIN) return NULL;

    return (char*)arena_push_bytes(gMem->temp, STB_SPRINTF_MIN);
}

// @HackLeaf: move to tprintf or whatever once that's there.
extern void
log_printf_(Log_Level level, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);

    char* buffer = push_stbsp_temp_storage(NULL, NULL, STB_SPRINTF_MIN);
    s32 written  = stbsp_vsprintfcb(push_stbsp_temp_storage, nullptr, buffer, fmt, va);

    gPlatform->log(level, buffer, written);

    // NOTE: not necessary b/c temp gets cleared every frame, but whatever.
    arena_pop(gMem->temp, written);
    va_end(va);
}
