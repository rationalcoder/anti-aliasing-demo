#pragma once
#include "platform.h"
#include "common.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat2x2.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

using v2 = glm::vec2;
using v3 = glm::vec3;
using v4 = glm::vec4;

using glm::mat2;
using glm::mat3;
using glm::mat4;


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

// TODO(blake): first class type. This is just so I can write code that won't
// have to change once I get around to this.
using string32 = buffer32;

template <typename T_> inline Array_View<T_, u32>
view_of(T_* data, u32 size) { return Array_View<T_, u32>(data, size); }

template <typename T_, u32 Size_> inline Array_View<T_, u32>
view_of(T_ (&data)[Size_]) { return Array_View<T_, u32>(data, Size_); }


template <typename Value_>
struct Value_Window
{
    Value_* values;
    u32     size;
    f64     average;

    u32     _oldest;
    f32     _valueWeight;

    Value_Window(ctor) {}
    Value_Window() : values(), size(), average(), _oldest(), _valueWeight() {}

    auto reset(Value_* values, u32 size) -> void;
    auto reset(Value_* values, u32 size, Value_ value) -> void;
    auto reset(Value_ value) -> void;

    auto add(Value_ val) -> void;

    // TODO(blake): foreach
};

template <typename Value_> void
Value_Window<Value_>::reset(Value_* values, u32 size)
{
    this->values  = values;
    this->size    = size;
    this->average = values[0];

    this->_oldest      = 0;
    this->_valueWeight = 1.0f/size;
}

template <typename Value_> void
Value_Window<Value_>::reset(Value_* values, u32 size, Value_ value)
{
    this->values  = values;
    this->size    = size;
    this->average = (f64)value;

    this->_oldest = 0;
    this->_valueWeight = 1.0f/size;

    for (u32 i = 0; i < size; i++)
        this->values[i] = value;
}

template <typename Value_> void
Value_Window<Value_>::reset(Value_ value)
{
    for (u32 i = 0; i < size; i++)
        values[i] = value;

    average = value;
    _oldest  = 0;
}

template <typename Value_> void
Value_Window<Value_>::add(Value_ value)
{
    average -= _valueWeight * values[_oldest];
    average += _valueWeight * value;

    values[_oldest] = value;
    right_rotate_value(_oldest, 0, size-1);
}

using u8_window  = Value_Window<u8>;
using u16_window = Value_Window<u16>;
using u32_window = Value_Window<u32>;
using u64_window = Value_Window<u64>;
using f32_window = Value_Window<f32>;
