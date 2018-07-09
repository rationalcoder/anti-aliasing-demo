#pragma once
#include "platform.h"
#include "primitives.h"

struct Memory_Arena
{
    const char* tag;

    Platform_Expand_Arena* expand;

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
};

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
arena_size(Memory_Arena& arena, umm elemSize = 1)
{
    umm result = ((u8*)arena.at - (u8*)arena.start) / elemSize;
    return result;
} 

// NOTE(blake): extra macros for now to enable FILE/LINE stuff later.

#define arena_reset arena_reset_

inline void
arena_reset_(Memory_Arena& arena)
{
    arena.at = arena.start;
}

inline void
arena_reset_(Memory_Arena& arena, void* to)
{
    arena.at = to;
}

#define arena_push arena_push_
#define arena_push_array(arena, n, type) (type*)arena_push_(arena, (n)*sizeof(type), alignof(type))
#define arena_push_type(arena, type) ((type*)arena_push_(arena, sizeof(type), alignof(type)))
#define arena_push_new(arena, type, ...) (new (arena_push_(arena, sizeof(type), alignof(type))) (type)(__VA_ARGS__))

inline void*
arena_push_(Memory_Arena& arena, umm size, u32 alignment)
{
    u8* at = (u8*)align_up(arena.at, alignment);
    u8* nextAt = at + size;

    if (nextAt >= (u8*)arena.next) {
        if (!arena.expand(&arena))
            return nullptr;
    }

    arena.at = nextAt;
    return at;
}

#define arena_push_bytes arena_push_bytes_

inline void*
arena_push_bytes_(Memory_Arena& arena, umm size)
{
    u8* at = (u8*)arena.at;
    u8* nextAt = at + size;

    if (nextAt >= (u8*)arena.next) {
        if (!arena.expand(&arena))
            return nullptr;
    }

    arena.at = nextAt;
    return at;
}

#define arena_sub_allocate arena_sub_allocate_

inline Memory_Arena
arena_sub_allocate_(Memory_Arena& arena, umm size, u32 alignment, const char* tag,
                    Platform_Expand_Arena* expand = gPlatform->failed_expand_arena)
{
    void* start = arena_push_(arena, size, alignment);

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

#define arena_sub_allocate_push_buffer arena_sub_allocate_push_buffer_

inline Push_Buffer
arena_sub_allocate_push_buffer_(Memory_Arena& arena, umm size, u32 alignment, const char* tag,
                                Platform_Expand_Arena* expand = gPlatform->failed_expand_arena)
{
    void* start = arena_push_(arena, size, alignment);

    Push_Buffer result;

    result.arena.tag    = tag;
    result.arena.expand = expand;
    result.arena.start  = start;
    result.arena.at     = start;
    result.arena.next   = (u8*)start + size;
    result.arena.size   = size;
    result.arena.max    = ~(umm)0; // max doesn't really make sense but inf is the most reasonable value.
    result.count        = 0;

    return result;;
}

#define arena_pop arena_pop_
#define arena_pop_type(arena, type) arena_pop_(arena, sizeof(type))

inline void
arena_pop_(Memory_Arena& arena, umm size)
{
    (u8*&)arena.at -= size;
}

template <typename T_>
struct Arena_Allocator
{
    Memory_Arena* arena = nullptr;

    T_* allocate(umm) { return arena_push_bytes(*arena, sizeof(T_)); }
    // @Hack: these can only be deallocated in order.
    void deallocate(T_*, umm) { arena_pop(*arena, sizeof(T_)); }

    bool operator == (Arena_Allocator<T_> rhs) { return arena == rhs.arena; }
    bool operator != (Arena_Allocator<T_> rhs) { return arena != rhs.arena; }
};

struct Memory_Arena_Scope
{
    Memory_Arena* arena;
    void* oldAt;

    Memory_Arena_Scope(Memory_Arena* arena) : arena(arena) { oldAt = arena->at; }
    ~Memory_Arena_Scope() { arena_reset(*arena, oldAt); }
};

#define arena_scope_impl(arena, counter) Memory_Arena_Scope _arenaScope##counter{&arena}
#define arena_scope(arena) arena_scope_impl(arena, __COUNTER__);

struct Push_Buffer_Scope
{
    Push_Buffer* buffer;
    void* oldAt;

    Push_Buffer_Scope(Push_Buffer* buffer) : buffer(buffer) { oldAt = buffer->arena.at; }
    ~Push_Buffer_Scope() { arena_reset(buffer->arena, oldAt); buffer->count = 0; }
};

#define push_buffer_scope_impl(buffer, counter) Push_Buffer_Scope _pushBufferScope##counter{&buffer}
#define push_buffer_scope(buffer) push_buffer_scope_impl(buffer, __COUNTER__);

// Extended Types

#if 0 // @Incomplete

template <typename T_, u32 Size_>
struct Bucket
{
    T_ data[Size_];
    Bucket<T_, Size_>* next;

    Bucket(ctor) {}
    Bucket(Bucket<T_, Size_>* next) : next(next) {}
};

template <typename T_, u32 Size_>
struct Bucket_Array
{
    using Bucket_Type = Bucket<T_, Size_>;
    Memory_Arena* arena;

    Bucket_Type* _head;
    Bucket_Type* _cur;
    T_*          _at;
    u32          _bucketCount;

    Bucket_Array(Memory_Arena& arena);
    Bucket_Array(ctor) {}

    void create_reserve(Memory_Arena& arena, u32 _bucketCount);
    void add(T_ val);
};

template <typename T_, u32 Size_> inline
Bucket_Array<T_, Size_>::Bucket_Array(Memory_Arena* arena)
    : arena(arena), _bucketCount(_bucketCount)
{
    _head = arena_push_type(*arena, Bucket_Type);
    _head->next = nullptr;

    _cur = _head;
    _at  = &_head->data[0];
}

template <typename T_, u32 Size_> inline void
Bucket_Array<T_, Size_>::create_reserve(Memory_Arena* arena, u32 bucketCount)
{
    _head = arena_push_type(*arena, Bucket_Type);
    _cur  = _head;
    _at   = (T_*)_head->data;

    for (u32 i = 0; i < bucketCount-1; i++) {
        Bucket_Type* newBucket = arena_push_type(*arena, Bucket_Type);
        _cur->next = newBucket;
        _cur       = newBucket;
    }

    _cur->next = nullptr;
    _at        = &_cur->data[0];

    _bucketCount = bucketCount;
}

template <typename T_, u32 Size_> inline void
Bucket_Array<T_, Size_>::add(T_ val)
{
}

#endif
