#pragma once
#include "common.h"
#include "memory.h"
#include "tanks.h"

template <typename T_, u32 Size_>
struct Bucket
{
    __declspec(align(alignof(T_))) char data[Size_ * sizeof(T_)];
    Bucket* next;

    Bucket() = default;
    Bucket(Bucket* next) : next(next) {}

    const T_* array() const { return (T_*)&data; }
    T_*       array()       { return (T_*)&data; }

    const T_* last() const { return (T_*)&data + (Size_-1); }
    T_*       last()       { return (T_*)&data + (Size_-1); }

    const T_* end() const { return (T_*)&data + Size_; }
    T_*       end()       { return (T_*)&data + Size_; }
};

template <typename T_, u32 Size_>
struct Bucket_Iterator
{
    using Bucket_Type = Bucket<T_, Size_>;

    Bucket_Type* _bucket;
    T_*          _cur;

    Bucket_Iterator(Bucket_Type* bucket, T_* cur)
        : _bucket(bucket), _cur(cur)
    {}

    bool operator == (Bucket_Iterator rhs) { return _cur == rhs._cur; }
    bool operator != (Bucket_Iterator rhs) { return _cur != rhs._cur; }

    const T_& operator  * () const { return *_cur; }
    T_&       operator  * ()       { return *_cur; }

    const T_* operator -> () const { return _cur; }
    T_*       operator -> ()       { return _cur; }

    Bucket_Iterator& operator ++ ()
    {
        if (_cur != _bucket->last()) {
            _cur++;
        }
        else {
            Bucket_Type* nextBucket = _bucket->next;
            _bucket = nextBucket;
            _cur    = (T_*)nextBucket;
        }

        return *this;
    }

    Bucket_Iterator operator ++ (int)
    {
        Bucket_Iterator result(_bucket, _cur);
        ++result;
        return result;
    }
};

// @Incomplete
template <typename T_, u32 BucketSize_ = 16>
struct Bucket_List
{
    using Bucket_Type    = Bucket<T_, BucketSize_>;
    using Iterator       = Bucket_Iterator<T_, BucketSize_>;
    using Const_Iterator = const Bucket_Iterator<T_, BucketSize_>;

    Bucket_Type* _head;
    Bucket_Type* _tail;
    T_*          _next;

    Memory_Arena* _arena;

    u32 _size;

    explicit Bucket_List(ctor) noexcept {}
    explicit Bucket_List(Memory_Arena& arena) noexcept;
    Bucket_List() noexcept : _head(), _tail(), _next(), _arena(), _size() {}

    void reset(Memory_Arena& arena) noexcept { new (this) Bucket_List<T_, BucketSize_>(arena); }

#if 0
    void push(T_& val) noexcept;
    void push(T_&& val) noexcept;
#endif

    T_* add_default() noexcept;
    T_* add_forget() noexcept;
    T_* add(T_& val) noexcept;
    T_* add(T_&& val) noexcept;

    u32 size() const noexcept { return _size; }

    const T_& back() const noexcept { return (_next-1); }
    T_&       back()       noexcept { return (_next-1); }

    Const_Iterator begin() const noexcept
    { return size() == 0 ? Iterator(nullptr, nullptr) : Iterator(_head, (T_*)_head); }

    Iterator       begin()       noexcept
    { return size() == 0 ? Iterator(nullptr, nullptr) : Iterator(_head, (T_*)_head); }

    Const_Iterator end() const noexcept
    { return size() == 0 ? Iterator(nullptr, nullptr) : Iterator(_tail, _next); }

    Iterator       end()       noexcept
    { return size() == 0 ? Iterator(nullptr, nullptr) : Iterator(_tail, _next); }

    Const_Iterator cbegin() const noexcept { return begin(); }
    Const_Iterator cend()   const noexcept { return end(); }
};

template <typename T_, u32 BucketSize_>
Bucket_List<T_, BucketSize_>::Bucket_List(Memory_Arena& arena) noexcept
    : _size(0)
{
    Bucket_Type* first = push_type(arena, Bucket_Type);

    _arena = &arena;
    _head  = first;
    _tail  = first;
    _next  = first->array();
}

template <typename T_, u32 BucketSize_> inline T_*
Bucket_List<T_, BucketSize_>::add_default() noexcept
{
    _size++;
    if (_next != _tail->end())
        return new (_next++) T_;

    Bucket_Type* newTail = push_type(*_arena, Bucket_Type);
    _tail->next = newTail;
    _tail       = newTail;
    _next       = newTail->array() + 1;

    return new (newTail->array()) T_;
}

template <typename T_, u32 BucketSize_> inline T_*
Bucket_List<T_, BucketSize_>::add_forget() noexcept
{
    _size++;
    if (_next != _tail->end())
        return _next++;

    Bucket_Type* newTail = push_type(*_arena, Bucket_Type);
    _tail->next = newTail;
    _tail       = newTail;
    _next       = newTail->array() + 1;

    return newTail->array();
}

template <typename T_, u32 BucketSize_> inline T_*
Bucket_List<T_, BucketSize_>::add(T_& val) noexcept
{
    _size++;
    if (_next != _tail->end())
        return new (_next++) T_(val);

    Bucket_Type* newTail = push_type(*_arena, Bucket_Type);
    _tail->next = newTail;
    _tail       = newTail;
    _next       = newTail->array() + 1;

    return new (newTail->array()) T_(val);
}

#if 0 // TODO(blake): std::move equivalent in common.h
template <typename T_, u32 BucketSize_> inline T_*
Bucket_List<T_, BucketSize_>::add(T_&& val) noexcept
{
    _size++;
    if (_next != _tail->end())
        return new (_next++) T_(move(val));

    Bucket_Type* newTail = push_type(*_arena, Bucket_Type);
    _tail->next = newTail;
    _tail       = newTail;
    _next       = newTail->array() + 1;

    return new (newTail->array()) T_(move(val));
}
#endif


#if 0
template <typename T_, u32 BucketSize_>
struct Bucket_Array
{
    using Bucket_Type = Bucket<T_, BucketSize_>;

    Memory_Arena* _arena;
    Bucket_Type*  _head;
    Bucket_Type*  _cur;
    T_*           _at;
    u32           _bucketCount;

    Bucket_Array(Memory_Arena& arena);
    Bucket_Array(ctor) {}

    void create_reserve(Memory_Arena& arena, u32 bucketCount);
    void add(T_ val);
};

template <typename T_, u32 BucketSize_> inline
Bucket_Array<T_, BucketSize_>::Bucket_Array(Memory_Arena* arena)
    : _arena(arena), _bucketCount(1)
{
    _head = push_type(*arena, Bucket_Type);
    _head->next = nullptr;

    _cur = _head;
    _at  = &_head->data[0];
}

template <typename T_, u32 BucketSize_> inline void
Bucket_Array<T_, BucketSize_>::create_reserve(Memory_Arena* arena, u32 bucketCount)
{
    _head = push_type(*arena, Bucket_Type);
    _cur  = _head;
    _at   = (T_*)_head->data;
    allocate_root(_head, 0);

    for (u32 i = 1; i < bucketCount; i++) {
        Bucket_Type* newBucket = push_type(*arena, Bucket_Type);
        _cur->next = newBucket;
        _cur       = newBucket;
        insert_bucket(newBucket, i);
    }

    _cur->next = nullptr;
    _at        = &_cur->data[0];

    _bucketCount = bucketCount;
}

template <typename T_, u32 BucketSize_> inline void
Bucket_Array<T_, BucketSize_>::add(T_ val)
{
    if (_at < _cur->end()) {
        *_at++ = val;
        return;
    }

    Bucket_Type* next = push_type(*_arena, Bucket_Type);
    _cur->next = next;
    _cur       = next;
    next->next = nullptr;

    T_* nextAt = &next->data[0];
    *nextAt    = val;
    _at        = nextAt;

    insert_bucket(next, _bucketCount++);
}

#endif

template <typename T_, u32 BucketSize_> inline T_*
flatten(const Bucket_List<T_, BucketSize_>& list)
{
    T_* flat = allocate_array(list.size(), T_);

    u32 i = 0;
    for (auto& e : list)
        flat[i++] = e;

    return flat;
}
