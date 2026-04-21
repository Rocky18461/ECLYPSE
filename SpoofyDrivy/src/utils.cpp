#include "utils.h"

extern "C" NTSTATUS ZwQuerySystemInformation(ULONG, PVOID, ULONG, PULONG);
extern "C" ULONG RtlRandomEx(PULONG);

namespace Utils {
    PVOID GetModuleBase(const char* name) {
        ULONG b = 0; ZwQuerySystemInformation(11, 0, 0, &b);
        MODULES* m = (MODULES*)ExAllocatePoolWithTag(NonPagedPool, b, ' ');
        if (!m || ZwQuerySystemInformation(11, m, b, &b)) return (m ? (void)ExFreePool(m) : (void)0), (PVOID)0;
        PVOID res = 0;
        for (ULONG i = 0; i < m->NumberOfModules; i++) {
            if (strstr((char*)m->Modules[i].FullPathName, name)) { res = m->Modules[i].ImageBase; break; }
        }
        ExFreePool(m); return res;
    }

    PVOID FindPatternImage(PVOID b, const char* p, const char* mc) {
        if (!b) return 0;
        auto nt = (PIMAGE_NT_HEADERS)((PUCHAR)b + ((PIMAGE_DOS_HEADER)b)->e_lfanew);
        auto sec = IMAGE_FIRST_SECTION(nt);
        for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
            if (sec[i].Characteristics & 0x20000000) {
                for (PUCHAR c = (PUCHAR)b + sec[i].VirtualAddress; c < (PUCHAR)b + sec[i].VirtualAddress + sec[i].Misc.VirtualSize - strlen(mc); c++) {
                    bool f = 1; for (int j = 0; mc[j]; j++) if (mc[j] == 'x' && c[j] != (UCHAR)p[j]) { f = 0; break; }
                    if (f) return c;
                }
            }
        }
        return 0;
    }

    void RandomBuffer(PUCHAR b, int l) {
        LARGE_INTEGER t; KeQueryTickCount(&t); ULONG s = t.LowPart;
        for (int i = 0; i < l; i++) b[i] = (UCHAR)(RtlRandomEx(&s) & 0xFF);
    }
}
