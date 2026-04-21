#include "tcpip.h"
#include "utils.h"
#include "log.h"

namespace Tcpip {
    NTSTATUS SpoofArp() {
        PVOID b = Utils::GetModuleBase("tcpip.sys"); if (!b) return 0;
        LARGE_INTEGER t; KeQueryTickCount(&t); ULONG s = t.LowPart;
        auto f = [&](PVOID g, bool v4) {
            if (!g || !MmIsAddressValid(g)) return;
            PLIST_ENTRY h = (PLIST_ENTRY)((PUCHAR)g + 0x4DC8);
            if (!MmIsAddressValid(h) || !MmIsAddressValid(h->Flink)) return;
            for (PLIST_ENTRY e = h->Flink; e != h && MmIsAddressValid(e); e = e->Flink) {
                PUCHAR ifc = (PUCHAR)e - 0xA0; PVOID ns = *(PVOID*)(ifc + 0x158);
                if (MmIsAddressValid(ns)) {
                    PLIST_ENTRY nl = (PLIST_ENTRY)((PUCHAR)ns + 0x38);
                    if (MmIsAddressValid(nl)) for (PLIST_ENTRY ne = nl->Flink; ne != nl && MmIsAddressValid(ne); ne = ne->Flink) {
                        PUCHAR n = (PUCHAR)ne - 0x40; PUCHAR ip = n + 0xA8, mc = n + (v4 ? 0xAC : 0xB8);
                        if (MmIsAddressValid(ip) && MmIsAddressValid(mc)) {
                            if (v4 && ip[3] == 1) continue;
                            for (int i = 3; i < 6; i++) mc[i] = (UCHAR)(RtlRandomEx(&s) & 0xFF);
                        }
                    }
                }
            }
        };
        PUCHAR i4 = (PUCHAR)Utils::FindPatternImage(b, ("\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x8B\xD8\x48\x85\xC0\x74\x00\x48\x8D\x54\x24\x00\x48\x8B\xC8"), ("xxx????x????xxxxxxx?xxxx?xxx"));
        if (i4) f((PVOID)(i4 + 7 + *(LONG*)(i4 + 3)), true);
        PUCHAR i6 = (PUCHAR)Utils::FindPatternImage(b, ("\x48\x8D\x15\x00\x00\x00\x00\x33\xC9\x00\x00\x00\x48\x89\x45"), ("xxx????xx???xxx"));
        if (i6) f((PVOID)(i6 + 7 + *(LONG*)(i6 + 3)), false);
        return 0;
    }
}
