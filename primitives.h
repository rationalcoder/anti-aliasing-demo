#pragma once
#include <stdlib.h>
#include "platform.h"

enum ctor { uninitialized };

// Bitsets, fields, and flags:

template <typename T>
struct Bitfield
{
    T flags = 0;

    void set(u32 i)   { flags |= (1 << i); }
    void unset(u32 i) { flags &= ~(1 << i); }
    void clear()      { flags = 0; }

    b32 is_set(u32 i) const { return flags & (1 << i); }
    b32 all()         const { return flags == ~(u32)0; }
    b32 none()        const { return flags == 0; }
};

using bit64 = Bitfield<u64>;
using bit32 = Bitfield<u32>;
using bit16 = Bitfield<u16>;
using bit8  = Bitfield<u8>;

using flag64 = u64;
using flag32 = u32;
using flag16 = u16;
using flag8  = u8;


template <u32 N_>
struct Bitset
{
    static constexpr u32 element_count() { return N_/sizeof(u32); }

    u32 _array[element_count()] = { 0 };

    void set(u32 i);
    void unset(u32 i);
    void clear() { memset(_array, 0, sizeof(_array)); }

    b32 is_set(u32 i) const;
    b32 all()         const;
    b32 none()        const;
};

template <u32 N_> void
Bitset<N_>::set(u32 i) 
{ 
    u32 idx = i >> 5; 
    _array[idx] |= (i - (idx << 5)); 
}

template <u32 N_> void
Bitset<N_>::unset(u32 i) 
{ 
    u32 idx = i >> 5; 
    _array[idx] &= ~(i - (idx << 5)); 
}

template <u32 N_> b32
Bitset<N_>::is_set(u32 i) const
{ 
    u32 idx = i >> 5; 
    return _array[idx] & (i - (idx << 5)); 
}

template <u32 N_> b32
Bitset<N_>::all() const
{ 
    for (u32 i = 0; i < element_count(); i++)
        if (_array[i] != ~(u32)0) return false;

    return true;
}

template <u32 N_> b32
Bitset<N_>::none() const
{ 
    for (u32 i = 0; i < element_count(); i++)
        if (_array[i] != 0) return false;

    return true;
}

// Views:

template <typename T_, typename Size_ = u32>
struct Array_View
{
    T_* data;
    Size_ size;

    Array_View(ctor) {}
    Array_View() : data(nullptr), size(0) {}
    Array_View(T_* data, Size_ size) : data(data), size(size) {}

    const T_& operator[](Size_ i) const { return data[i]; }
     T_&      operator[](Size_ i)       { return data[i]; }

    const T_* begin() const { return data; }
    T_*       begin()       { return data; }

    const T_* end() const { return data + size; }
    T_*       end()       { return data + size; }

    const T_* cbegin() const { return data; }
    T_*       cend()   const { return data + size; }

    operator bool() { return data != nullptr; }
};

template <typename T_> using view8  = Array_View<T_, u8>;
template <typename T_> using view16 = Array_View<T_, u16>;
template <typename T_> using view32 = Array_View<T_, u32>;
template <typename T_> using view64 = Array_View<T_, u64>;

using buffer8  = Array_View<u8, u8>;
using buffer16 = Array_View<u8, u16>;
using buffer32 = Array_View<u8, u32>;
using buffer64 = Array_View<u8, u64>;

template <typename T_, typename Size_> inline Array_View<T_, Size_>
view_of(T_* data, Size_ size) { return Array_View<T_, Size_>(data, size); }

template <typename T_, u32 Size_> inline Array_View<T_, u32>
view_of(T_ (&data)[Size_]) { return Array_View<T_, u32>(data, Size_); }

