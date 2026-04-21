#include "smbios.h"
#include "utils.h"

extern "C" ULONG RtlRandomEx(PULONG);

namespace Smbios {
    char* GetStr(SMBIOS_HEADER* h, int s) {
        char* p = (char*)h + h->Length;
        for (; --s > 0; p++) while (*p) p++;
        return p;
    }

    bool IsDef(char* s) {
        const char* b[] = { ("to be"), ("def"), ("oem"), ("syst"), ("applic"), ("none"), ("0000"), ("1234") };
        if (!s || !*s) return 1;
        for (int i = 0; i < 8; i++) for (char* c = s; *c; c++) {
            int j = 0; while (b[i][j] && (c[j] | 32) == (b[i][j] | 32)) j++;
            if (!b[i][j]) return 1;
        }
        return 0;
    }

    void RandStr(char* s) {
        if (IsDef(s)) return;
        ULONG seed = (ULONG)KeQueryInterruptTime();
        for (; *s; s++) {
            if ((*s | 32) >= 'a' && (*s | 32) <= 'z') *s = (*s & 96) + 1 + RtlRandomEx(&seed) % 26;
            else if (*s >= '0' && *s <= '9') *s = '0' + RtlRandomEx(&seed) % 10;
            else if (*s > 32) *s = ("_.-#")[RtlRandomEx(&seed) % 4];
        }
    }

    NTSTATUS ChangeSmbiosSerials() {
        PVOID b = Utils::GetModuleBase(("ntoskrnl.exe"));
        PUCHAR p = (PUCHAR)Utils::FindPatternImage(b, ("\x48\x8B\x0D\x00\x00\x00\x00\x48\x85\xC9\x74\x00\x8B\x15\x00\x00\x00\x00\x44\x8D\x43"), ("xxx????xxxx?xx????xxx"));
        PUCHAR s = (PUCHAR)Utils::FindPatternImage(b, ("\x8B\x05\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xEB\x00\xBF"), ("xx??????????x?x"));
        if (!p || !s) return 0;
        PHYSICAL_ADDRESS pa = *(PHYSICAL_ADDRESS*)(p + 7 + *(int*)(p + 3));
        ULONG sz = *(ULONG*)(s + 6 + *(int*)(s + 2));
        PUCHAR m = (PUCHAR)MmMapIoSpace(pa, sz, MmNonCached);
        if (m) {
            for (PUCHAR cur = m; cur < m + sz; ) {
                SMBIOS_HEADER* h = (SMBIOS_HEADER*)cur;
                if (h->Type == 127) break;
                if (h->Type == 1) { RandStr(GetStr(h, ((SMBIOS_TYPE1*)h)->SerialNumber)); Utils::RandomBuffer(((SMBIOS_TYPE1*)h)->UUID, 16); }
                if (h->Type == 2) RandStr(GetStr(h, ((SMBIOS_TYPE2*)h)->SerialNumber));
                if (h->Type == 3) RandStr(GetStr(h, ((SMBIOS_TYPE3*)h)->SerialNumber));
                if (h->Type == 4) { RandStr(GetStr(h, ((SMBIOS_TYPE4*)h)->SerialNumber)); Utils::RandomBuffer((PUCHAR)&((SMBIOS_TYPE4*)h)->ProcessorId, 8); }
                PUCHAR end = cur + h->Length;
                while (*end || *(end + 1)) end++;
                cur = end + 2;
            }
            MmUnmapIoSpace(m, sz);
        }
        return 0;
    }
}
