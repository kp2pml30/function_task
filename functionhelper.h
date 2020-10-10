#pragma once

#include <exception>
#include <type_traits>

#include <experimental/type_traits>

class bad_function_call : public std::exception { public: using std::exception::exception; };

template <typename F>
struct function;

template<typename T, bool>
struct FunctionTraits;

template<typename T>
constexpr bool IsSmallObject(void)
{
    return
        sizeof(T) <= sizeof(void*) &&
        alignof(void*) % alignof(T) == 0 &&
        std::is_nothrow_constructible_v<T, T&&>;
}

using FunctionBuffer = std::aligned_storage_t<sizeof(void*), alignof(void*)>;

template<typename R, typename... Args>
struct TypeDescriptor;

template<typename R, typename... Args>
struct FunctionStorage
{
private:
    template<typename Y>
    friend class function;
    template<typename Y, bool>
    friend class FunctionTraits;
    FunctionBuffer buf;
    TypeDescriptor<R, Args...> const* desc;
public:
    FunctionStorage(void) = default;

    template<typename T>
    T* SmallCast(void)
    {
        return reinterpret_cast<T *>(&buf);
    }
    template<typename T>
    T const* SmallCast(void) const
    {
        return reinterpret_cast<const T*>(&buf);
    }
    template<typename T>
    T* BigCast(void) const
    {
        using Tptr = T*;
        return *reinterpret_cast<Tptr const*>(&buf);
    }
};

template<typename R, typename... Args>
struct TypeDescriptor
{
private:
    using FunctionStorage = ::FunctionStorage<R, Args...>;
public:
    void (*Destroy)(FunctionStorage *del);
    void (*Move)(FunctionStorage *to, FunctionStorage *from);
    void (*Copy)(FunctionStorage *to, FunctionStorage const* from);
    R (*Invoke)(FunctionStorage const* what, Args...);
};

template<typename R, typename... Args>
TypeDescriptor<R, Args...> const* EmptyTypeDescriptor(void)
{
    using FunctionStorage = ::FunctionStorage<R, Args...>;
    constexpr static TypeDescriptor<R, Args...> ret =
    {
        +[](FunctionStorage* del) {},
        +[](FunctionStorage* to, FunctionStorage* from) { *to = *from; },
        +[](FunctionStorage* to, FunctionStorage const* from) { *to = *from; },
        +[](FunctionStorage const* what, Args...) -> R { throw bad_function_call(); }
    };
    return &ret;
}

// small object
template<typename T>
struct FunctionTraits<T, true>
{
public:
    template<typename R, typename... Args>
    static TypeDescriptor<R, Args...> const * Descr(void) noexcept
    {
        using FunctionStorage = ::FunctionStorage<R, Args...>;
        constexpr static TypeDescriptor<R, Args...> ret =
        {
            +[](FunctionStorage* del)
            {
                del->template SmallCast<T>()->~T();
            },
            +[](FunctionStorage* to, FunctionStorage* from)
            {
                new(&to->buf) T(std::move(*from->template SmallCast<T>()));
                to->desc = from->desc;
            },
            +[](FunctionStorage* to, FunctionStorage const* from)
            {
                new(&to->buf) T(*from->template SmallCast<T>());
                to->desc = from->desc;
            },
            +[](FunctionStorage const* what, Args... args) -> R
            {
                // here is a const problem with small object optimization
                if constexpr (0)
                    return what->template SmallCast<T>()->operator()(std::forward<Args>(args)...);
                else
                    return const_cast<T *>(what->template SmallCast<T>())->operator()(std::forward<Args>(args)...);
            }
        };
        return &ret;
    }
};
// big object
template<typename T>
struct FunctionTraits<T, false>
{
    template<typename R, typename... Args>
    static TypeDescriptor<R, Args...> const* Descr(void) noexcept
    {
        using FunctionStorage = ::FunctionStorage<R, Args...>;
        constexpr static TypeDescriptor<R, Args...> ret =
        {
            +[](FunctionStorage *del)
            {
                delete del->template BigCast<T>();
            },
            +[](FunctionStorage *to, FunctionStorage *from)
            {
                *reinterpret_cast<T**>(&to->buf) = from->template BigCast<T>();
                to->desc = from->desc;
                from->desc = EmptyTypeDescriptor<R, Args...>();
            },
            +[](FunctionStorage *to, FunctionStorage const* from)
            {
                *reinterpret_cast<T**>(&to->buf) = new T(*from->template BigCast<T>());
                to->desc = from->desc;
            },
            +[](const FunctionStorage *what, Args... args) -> R
            {
                return what->template BigCast<T>()->operator()(std::forward<Args>(args)...);
            }
        };
        return &ret;
    }
};