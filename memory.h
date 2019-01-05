#pragma once
#include <memory>

#include "platform.h"
#include "primitives.h"

struct Memory_Arena
{
    const char* tag;

    Platform_Expand_Arena* expand;
    void* user;

    void* start;
    void* at;
    void* next; 

    umm size;
    umm max;
};

struct Push_Buffer
{
    Memory_Arena arena;
    u32 count;

    Push_Buffer& operator = (Memory_Arena arena) noexcept;
};

inline Push_Buffer&
Push_Buffer::operator = (Memory_Arena arena) noexcept
{
    this->arena = arena;
    this->count = 0;
    return *this;
}

inline b32
is_aligned(void* address, s32 alignPow2)
{
    bool result = ((uptr)address & (alignPow2-1)) == 0;
    return result;
}

inline void*
align_up(void* address, s32 alignPow2)
{
    void* result = (void*)(((uptr)address + alignPow2-1) & -alignPow2);
    return result;
}

inline void*
align_down(void* address, s32 alignPow2)
{
    void* result = (void*)((uptr)address & ~(alignPow2-1));
    return result;
}

template <typename T_> inline T_*
next_header(T_* headerAddress, u32 offset)
{
    T_* result = (T_*)aligned_offset(headerAddress + 1, offset, alignof(T_));
    return result;
}

template <typename T_> inline void*
payload_after(T_* headerAddress)
{
    return headerAddress + 1;
}

inline void*
aligned_offset(void* address, u32 offset, s32 alignPow2)
{
    void* result = align_up((u8*)address + offset, alignPow2);
    return result;
}

inline umm
arena_size(Memory_Arena& arena)
{
    umm result = ((u8*)arena.at - (u8*)arena.start);
    return result;
} 

inline umm
arena_size(Memory_Arena& arena, umm elemSize)
{
    //umm result = ((u8*)arena.at - (u8*)arena.start) / elemSize;
    umm result = arena_size(arena) / elemSize;
    return result;
} 

inline void
reset(Memory_Arena& arena)
{
    arena.at = arena.start;
}

inline void
reset(Memory_Arena& arena, void* to)
{
    arena.at = to;
}

inline void
reset(Push_Buffer& buffer)
{
    reset(buffer.arena);
    buffer.count = 0;
}

#define push_array(arena, n, type) (type*)push(arena, (n)*sizeof(type), alignof(type))
#define push_array_copy(arena, n, type, data) (type*)push_copy(arena, (n)*sizeof(type), alignof(type), data)
#define push_type(arena, type) ((type*)push(arena, sizeof(type), alignof(type)))
#define push_new(arena, type, ...) (new (push(arena, sizeof(type), alignof(type))) (type)(__VA_ARGS__))

inline void*
push(Memory_Arena& arena, umm size, u32 alignment)
{
    u8* at = (u8*)align_up(arena.at, alignment);
    u8* nextAt = at + size;

    if (nextAt > (u8*)arena.next) {
        if (!arena.expand(&arena, nextAt - (u8*)arena.at))
            return nullptr;
    }

    arena.at = nextAt;
    return at;
}

inline void*
push_bytes(Memory_Arena& arena, umm size)
{
    u8* at = (u8*)arena.at;
    u8* nextAt = at + size;

    if (nextAt > (u8*)arena.next) {
        if (!arena.expand(&arena, size))
            return nullptr;
    }

    arena.at = nextAt;
    return at;
}

inline void*
push_zero(Memory_Arena& arena, umm size, u32 alignment)
{
    void* space = push(arena, size, alignment);
    memset(space, 0, size);

    return space;
}

inline void*
push_copy(Memory_Arena& arena, umm size, u32 alignment, void* data)
{
    void* space = push(arena, size, alignment);
    memcpy(space, data, size);

    return space;
}

#define sub_allocate(...) sub_allocate_(__VA_ARGS__)

inline Memory_Arena
sub_allocate_(Memory_Arena& arena, umm size, u32 alignment, const char* tag,
             Platform_Expand_Arena* expand = gPlatform->failed_expand_arena)
{
    void* start = push(arena, size, alignment);

    Memory_Arena result;
    result.tag    = tag;
    result.expand = expand;
    result.start  = start;
    result.at     = start;
    result.next   = (u8*)start + size;
    result.size   = size;
    result.max    = ~(umm)0; // max doesn't really make sense but inf is the most reasonable value.
    
    return result;
}

#define pop_type(arena, type) pop(arena, sizeof(type))

inline void
pop(Memory_Arena& arena, umm size)
{
    (u8*&)arena.at -= size;
}

template <typename T_>
struct Arena_Allocator
{
    Memory_Arena* arena = nullptr;

    T_* allocate(umm) { return push_bytes(*arena, sizeof(T_)); }
    // @Hack: these can only be deallocated in order.
    void deallocate(T_*, umm) { pop(*arena, sizeof(T_)); }

    bool operator == (Arena_Allocator<T_> rhs) { return arena == rhs.arena; }
    bool operator != (Arena_Allocator<T_> rhs) { return arena != rhs.arena; }
};

struct Memory_Arena_Scope
{
    Memory_Arena* arena;
    void* oldAt;

    Memory_Arena_Scope(Memory_Arena* arena) : arena(arena) { oldAt = arena->at; }
    ~Memory_Arena_Scope() { reset(*arena, oldAt); }
};

#define arena_scope_impl(arena, counter) Memory_Arena_Scope _arenaScope##counter{&arena}
#define arena_scope(arena) arena_scope_impl(arena, __COUNTER__);

struct Push_Buffer_Scope
{
    Push_Buffer* buffer;
    void* oldAt;

    Push_Buffer_Scope(Push_Buffer* buffer) : buffer(buffer) { oldAt = buffer->arena.at; }
    ~Push_Buffer_Scope() { reset(buffer->arena, oldAt); buffer->count = 0; }
};

#define push_buffer_scope_impl(buffer, counter) Push_Buffer_Scope _pushBufferScope##counter{&buffer}
#define push_buffer_scope(buffer) push_buffer_scope_impl(buffer, __COUNTER__);

using Allocate_Func = void* (void* data, umm size, u32 alignment);

struct Allocator
{
    Allocate_Func* func = nullptr;
    void*          data = nullptr;

    Allocator() = default;
    Allocator(Allocate_Func* func, void* data) : func(func), data(data) {}
};

inline void*
arena_allocate(void* arena, umm size, u32 alignment)
{ return push(*(Memory_Arena*)(arena), size, alignment); }

// Extended Types

// Simple, tiny list stuff for API boundaries
#define list_push(list, p) (((p)->next = (list)), p)

