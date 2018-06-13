#pragma once

#ifdef _WIN64
    #include "win32_tanks.h"
#else
    #error "Unsupported platform"
#endif

#include <stdint.h>

// NOTE(bmartin): the use of char here is mostly a joke; apparently, the int8 types
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

// TODO: fast ints, least ints, etc.

using f32 = float;
using f64 = double;


struct Platform
{
    b32 (*expand_arena)(struct Memory_Arena* arena) = nullptr;
};

