
extern void
log_printf_(Log_Level level, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);

    // NOTE(blake): this keeps infinite loops of logging from overflowing the temp arena.
    temp_scope();

    char* buffer = push_stbsp_temp_storage(NULL, NULL, STB_SPRINTF_MIN);
    s32 written  = stbsp_vsprintfcb(push_stbsp_temp_storage, nullptr, buffer, fmt, va);

    gPlatform->log(level, buffer, written);

    va_end(va);
}
