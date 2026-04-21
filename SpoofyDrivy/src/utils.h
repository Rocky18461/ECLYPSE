#pragma once
#include <ntifs.h>
#include <ntimage.h>

struct MODULE_INFO { PVOID Section, MappedBase, ImageBase; ULONG ImageSize, Flags; USHORT LoadOrder, InitOrder, LoadCount, Offset; UCHAR FullPathName[256]; };
struct MODULES { ULONG NumberOfModules; MODULE_INFO Modules[1]; };

namespace Utils {
    PVOID GetModuleBase(const char* name);
    PVOID FindPatternImage(PVOID base, const char* pattern, const char* mask);
    void RandomBuffer(PUCHAR buffer, int length);
}
