#include "ndis.h"
#include "utils.h"

extern "C" ULONG RtlRandomEx(PULONG);

namespace Ndis {
    NTSTATUS SpoofMAC() {
        PVOID b = Utils::GetModuleBase("ndis.sys");
        PUCHAR instr = (PUCHAR)Utils::FindPatternImage(b, ("\x48\x8B\x1D\x00\x00\x00\x00\x45\x33\xF6\x44\x0F\xB6\xF8\x48\x85\xDB"), ("xxx????xxxxxxxxx"));
        if (!instr) return 0;
        PVOID* list = (PVOID*)(instr + 7 + *(LONG*)(instr + 3));
        if (!list || !*list) return 0;
        LARGE_INTEGER t; KeQueryTickCount(&t); ULONG seed = t.LowPart;
        for (PUCHAR mini = (PUCHAR)*list; mini; mini = (PUCHAR)*(PVOID*)(mini + 0xF08)) {
            PUCHAR ifb = (PUCHAR)*(PVOID*)(mini + 0xFC8);
            if (ifb && MmIsAddressValid(ifb)) {
                PUCHAR pm = ifb + 0x488, cm = ifb + 0x466;
                if (MmIsAddressValid(pm) && MmIsAddressValid(cm)) for (int i = 3; i < 6; i++) pm[i] = cm[i] = (UCHAR)(RtlRandomEx(&seed) & 0xFF);
            }
        }
        return 0;
    }
}
