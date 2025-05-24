#pragma once

#include <Windows.h>
#include <Psapi.h>
#include <string.h>
#include <stdint.h>
#include <vector>

struct signature_t
{
    uint8_t* m_ptr = nullptr;

    template <typename T> T as(uintptr_t offset = 0)
    {
        return reinterpret_cast<T>(this->m_ptr + offset);
    }

    [[nodiscard]] uint8_t* add(const uint8_t offset) const
    {
        return this->m_ptr + offset;
    }

    __forceinline signature_t rel32(size_t offset)
    {
        uint8_t* out;
        uint32_t r;

        out = m_ptr + offset;

        // get rel32 offset.
        r = *(uint32_t*)out;
        if (!r)
            return { nullptr };

        // relative to address of next instruction.
        out = (out + 4) + r;

        return { (uint8_t*)out };
    }
};

signature_t get_sig(const char* mod_name, const char* sig);