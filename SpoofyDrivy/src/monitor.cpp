#include "monitor.h"
#include "utils.h"

extern "C" ULONG RtlRandomEx(PULONG);

namespace Monitor {
    void Spoof(PUCHAR a, ULONG& s) {
        if (!a) return;
        PUCHAR e = 0;
        for (int i = 0; i < 48; i++) if (!memcmp(a + i, ("\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00"), 8)) { e = a + i; break; }
        if (!e) return;
        *(ULONG*)(e + 12) = RtlRandomEx(&s);
        for (int i = 54; i < 110; i += 18) {
            if (!memcmp(e + i, ("\x00\x00\x00\xFF"), 4)) {
                for (int j = 0; j < 10; j++) e[i + j + 5] = ("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ")[RtlRandomEx(&s) % 36];
                break;
            }
        }
        UCHAR c = 0; for (int j = 0; j < 127; j++) c += e[j];
        e[127] = (UCHAR)(0x100 - c);
    }

    NTSTATUS SpoofMonitors() {
        PVOID b = Utils::GetModuleBase("dxgkrnl.sys");
        PUCHAR p = (PUCHAR)Utils::FindPatternImage(b, ("\x48\x8B\x0D\x00\x00\x00\x00\x48\x85\xC9\x74\x00\xBA\x01\x00\x00\x00"), ("xxx????xxxx?xxxxx"));
        if (!p) return 0;
        LARGE_INTEGER t; KeQueryTickCount(&t); ULONG s = t.LowPart;
        PUCHAR* cp = (PUCHAR*)(p + 7 + *(int*)(p + 3));
        if (cp && *cp) for (int i = 0; i < 4; i++) {
            PUCHAR e = *cp + 8 + (i * 152);
            if (*(ULONGLONG*)(e + 8)) Spoof(e, s);
        }
        return 0;
    }
}
