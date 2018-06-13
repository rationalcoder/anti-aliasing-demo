#pragma once
#include <assert.h>
#include "platform.h"

// NOTE: needed a place to put things to avoid having demo.h include tanks.h

#define Gigabytes(x) (x * (1ULL << 30))
#define Megabytes(x) (x * (1ULL << 20))
#define Kilobytes(x) (x * (1ULL << 10))

#define InvalidCodePath() assert(!"Invalid code path.");
#define ArraySize(arr) (sizeof(arr)/sizeof((arr)[0]))

#define FieldCount(type, field) (offsetof(type, field)/sizeof(field))
#define AssertFirstField(type, field)\
static_assert(offsetof(type, field) == 0, "Field '" #field "' must come first in '" #type "'")

#define CONCAT(a, b) a##b

#define FIELD_ARRAY_IMPL(type, name, structDefinition, counter)\
typedef struct structDefinition CONCAT(_anon_array, counter);\
union {\
    struct structDefinition;\
    type name[sizeof(CONCAT(_anon_array, counter))/sizeof(type)];\
};\
static_assert(sizeof(CONCAT(_anon_array, counter)) % sizeof(type) == 0,\
              "Field Array of type '" #type "' must be a multiple of sizeof(" #type ")")\

#define FIELD_ARRAY(type, name, structDefinition)\
FIELD_ARRAY_IMPL(type, name, structDefinition, __COUNTER__)

struct Game_Resolution
{
    u32 w;
    u32 h;
};

inline bool
operator == (Game_Resolution lhs, Game_Resolution rhs)
{
    return lhs.w == rhs.w && lhs.h == rhs.h;
}

inline bool
operator != (Game_Resolution lhs, Game_Resolution rhs)
{
    return !(lhs == rhs);
}

template <typename To_, typename From_> static inline To_
checked_downcast(From_ from)
{
    To_ result = (To_)from;
    assert(result == from);

    return result;
}
