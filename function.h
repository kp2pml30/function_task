#pragma once

#include "functionhelper.h"

template <typename R, typename... Args>
struct function<R (Args...)>
{
private:
    FunctionStorage<R, Args...> storage;
public:
    function() noexcept
    {
        storage.desc = EmptyTypeDescriptor<R, Args...>();
    }

    function(function const& other)
    {
        other.storage.desc->Copy(&storage, &other.storage);
    }
    function(function&& other) noexcept
    {
        other.storage.desc->Move(&storage, &other.storage);
    }

    template <typename T>
    function(T val)
    {
        if constexpr (IsSmallObject<T>())
            new(&storage.buf) T(std::move(val));
        else
            *reinterpret_cast<const T **>(&storage.buf) = new T(std::move(val));
        storage.desc = FunctionTraits<T, IsSmallObject<T>()>::template Descr<R, Args...>();
    }

    function& operator=(function const& rhs)
    {
        if (this == &rhs)
            return *this;
        decltype(storage.buf) copy = storage.buf;
        decltype(storage.desc) dcopy = storage.desc;
        try
        {
            rhs.storage.desc->Copy(&storage, &rhs.storage);
            // noexcept part
            storage.desc = std::move(dcopy);
            std::swap(copy, storage.buf);
            storage.desc->Destroy(&storage);
            storage.buf = copy;
            storage.desc = rhs.storage.desc;
        }
        catch (...)
        {
            // storage.buf = copy; // copy has not changed anything
            throw;
        }
        return *this;
    }
    function& operator=(function&& rhs) noexcept
    {
        if (this == &rhs)
            return *this;
        storage.desc->Destroy(&storage);
        storage.desc = EmptyTypeDescriptor<R, Args...>();
        rhs.storage.desc->Move(&storage, &rhs.storage);
        return *this;
    }

    ~function()
    {
        storage.desc->Destroy(&storage);
    }

    explicit operator bool() const noexcept
    {
        return storage.desc != EmptyTypeDescriptor<R, Args...>();
    }

    R operator()(Args... args) const
    {
        return storage.desc->Invoke(&storage, std::forward<Args>(args)...);
    }

    template <typename T>
    T* target() noexcept
    {
        if (FunctionTraits<T, IsSmallObject<T>()>::template Descr<R, Args...>() != storage.desc)
            return nullptr;
        if constexpr (IsSmallObject<T>())
            return storage.template SmallCast<T>();
        else
            return storage.template BigCast<T>();
    }

    template <typename T>
    T const* target() const noexcept
    {
        if (FunctionTraits<T, IsSmallObject<T>()>::template Descr<R, Args...>() != storage.desc)
            return nullptr;
        if constexpr (IsSmallObject<T>())
            return storage.template SmallCast<T>();
        else
            return storage.template BigCast<T>();
    }
};
