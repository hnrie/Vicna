#pragma once

#include <Windows.h>

template<typename T>
struct VMValue0
{
private:
    T Storage;

public:
    operator const T() const
    {
        return Storage;
    }

    void operator=(const T& Value)
    {
        Storage = Value;
    }

    const T operator->() const
    {
        return Storage;
    }

    T Get() const
    {
        return Storage;
    }

    void Set(const T& Value)
    {
        Storage = Value;
    }
};

template<typename T>
struct VMValue1
{
private:
    T Storage;

public:
    operator const T() const
    {
        return (T)((uintptr_t)Storage - (uintptr_t)this);
    }

    void operator=(const T& Value)
    {
        Storage = (T)((uintptr_t)Value + (uintptr_t)this);
    }

    const T operator->() const
    {
        return operator const T();
    }

    T Get() const
    {
        return operator const T();
    }

    void Set(const T& Value)
    {
        operator=(Value);
    }
};

template<typename T>
struct VMValue2
{
private:
    T Storage;

public:
    operator const T() const
    {
        return (T)((uintptr_t)this - (uintptr_t)Storage);
    }

    void operator=(const T& Value)
    {
        Storage = (T)((uintptr_t)this - (uintptr_t)Value);
    }

    const T operator->() const
    {
        return operator const T();
    }

    T Get() const
    {
        return operator const T();
    }

    void Set(const T& Value)
    {
        operator=(Value);
    }
};

template<typename T>
struct VMValue3
{
private:
    T Storage;

public:
    operator const T() const
    {
        return (T)((uintptr_t)this ^ (uintptr_t)Storage);
    }

    void operator=(const T& Value)
    {
        Storage = (T)((uintptr_t)Value ^ (uintptr_t)this);
    }

    const T operator->() const
    {
        return operator const T();
    }

    T Get() const
    {
        return operator const T();
    }

    void Set(const T& Value)
    {
        operator=(Value);
    }
};

template<typename T>
struct VMValue4
{
private:
    T Storage;

public:
    operator const T() const
    {
        return (T)((uintptr_t)this + (uintptr_t)Storage);
    }

    void operator=(const T& Value)
    {
        Storage = (T)((uintptr_t)Value - (uintptr_t)this);
    }

    const T operator->() const
    {
        return operator const T();
    }

    T Get() const
    {
        return operator const T();
    }

    void Set(const T& Value)
    {
        operator=(Value);
    }
};