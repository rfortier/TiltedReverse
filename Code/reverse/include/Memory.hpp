#pragma once

#include <Windows.h>
#include <AutoPtr.hpp>

#include <mem/mem.h>
#include <mem/module.h>

#include <MemoryVP.hpp>

namespace TiltedPhoques
{
    using TNameHandler = void(*)(void*, const char*);

    namespace detail
    {
        // dumbs down a class member ptr to a generic
        // void ptr, this is technically undefined behavior
        // but is verified to always work on MSVC
        // https://stackoverflow.com/questions/8121320/get-memory-address-of-member-function
        template<typename TOut, typename TIn>
        TOut PunType(TIn in)
        {
            union
            {
                TIn  in;
                TOut out;
            }
            u = { in };

            return u.out;
        };

        inline static mem::pointer currentBase;
        inline static TNameHandler nameHandler = nullptr;
    }

    class BaseOverrideContext final
    {
    public:
        inline BaseOverrideContext(const mem::pointer aNewPtr)
        {
            m_prevBase = detail::currentBase;
            detail::currentBase = aNewPtr;
        }

        inline ~BaseOverrideContext()
        {
            detail::currentBase = m_prevBase;
        }
    private:
        mem::pointer m_prevBase;
    };

    inline void SetBase(const mem::pointer acBase, const mem::module* apOptModule)
    {
        const mem::pointer disp = apOptModule ? acBase - apOptModule->start : acBase - 0x140000000;
        detail::currentBase = disp.as<ptrdiff_t>();
    }

    constexpr bool IsVAOnly(const mem::pointer aEa)
    {
        // don't tune fixed addresses, which are bigger than a VA.
        return aEa <= UINT_MAX;
    }

    MEM_STRONG_INLINE void TuneBase(mem::pointer &arAddress)
    {
        arAddress.add(detail::currentBase.as<size_t>());
    }

    template<typename T>
    MEM_STRONG_INLINE void Put(mem::pointer aEa, const T acVal) noexcept
    {
        if (IsVAOnly(aEa)) TuneBase(aEa);

        if constexpr(std::is_pod_v<T> && sizeof(T) < sizeof(uintptr_t))
            // directly set value if it fits within one register
            *reinterpret_cast<T*>(aEa.as<uintptr_t>()) = acVal;
        else
            std::memcpy(aEa.as<void*>(), &acVal, sizeof(T));
    }

    MEM_STRONG_INLINE void Nop(mem::pointer aEa, size_t aLength) noexcept
    {
        if (IsVAOnly(aEa)) TuneBase(aEa);
        mem::region(aEa, aLength).fill(0x90);
    }

    // jump function
    template<typename T>
    MEM_STRONG_INLINE constexpr void Jump(const mem::pointer aEa, const T& aFunc) noexcept
    {
        if (IsVAOnly(aEa)) TuneBase(aEa);
        Put<uint8_t>(aEa, 0xE9);
        Put<int32_t>(aEa.as<intptr_t>() + 1, (intptr_t)aFunc - aEa.as<intptr_t>() - 5);
    }

    // jump class member function
    template<typename T>
    MEM_STRONG_INLINE constexpr void JumpCMF(const mem::pointer aEa, const T& aFunc) noexcept
    {
        if (IsVAOnly(aEa)) TuneBase(aEa);
        Jump(aEa, detail::PunType<const void*>(aFunc));
    }

    // Support code for reflection - don't use
    MEM_STRONG_INLINE void SetNameHandler(TNameHandler aHandler) { detail::nameHandler = aHandler; }
    MEM_STRONG_INLINE void InvokeNameHandler(const char* apcName) { detail::nameHandler(nullptr, apcName); }
}
