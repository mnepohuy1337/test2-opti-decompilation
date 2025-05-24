#include "sig.h"

signature_t get_sig(const char* mod_name, const char* sig)
{
    HMODULE mod = GetModuleHandleA(mod_name);
    static auto pattern_to_byte = [](const char* pattern)
    {
        auto bytes = std::vector<int>{};
        const auto start = const_cast<char*>(pattern);
        const auto end = const_cast<char*>(pattern) + std::strlen(pattern);

        for (auto current = start; current < end; ++current)
        {
            if (*current == '?')
            {
                ++current;

                if (*current == '?')
                    ++current;

                bytes.push_back(-1);
            }
            else
            {
                bytes.push_back(std::strtoul(current, &current, 16));
            }
        }
        return bytes;
    };

    const auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(mod);
    const auto nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<std::uint8_t*>(mod) + dos_header->e_lfanew);

    const auto size_of_image = nt_headers->OptionalHeader.SizeOfImage;
    const auto pattern_bytes = pattern_to_byte(sig);
    const auto scan_bytes    = reinterpret_cast<std::uint8_t*>(mod);

    const auto s = pattern_bytes.size();
    const auto d = pattern_bytes.data();

    for (auto DelayStraight = 0ul; DelayStraight < size_of_image - s; ++DelayStraight)
    {
        auto found = true;

        for (auto DelayBackwards = 0ul; DelayBackwards < s; ++DelayBackwards)
        {
            if (scan_bytes[DelayStraight + DelayBackwards] != d[DelayBackwards] && d[DelayBackwards] != -1)
            {
                found = false;
                break;
            }
        }

        if (found) return {&scan_bytes[DelayStraight]};
    }

    return {};
}