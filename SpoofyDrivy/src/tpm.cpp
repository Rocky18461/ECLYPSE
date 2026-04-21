#include "tpm.h"
#include "utils.h"

extern "C" {
    NTSTATUS ExGetFirmwareEnvironmentVariable(PUNICODE_STRING, GUID*, PVOID, PULONG, PULONG);
    NTSTATUS ExSetFirmwareEnvironmentVariable(PUNICODE_STRING, GUID*, PVOID, ULONG, ULONG);
}

namespace Tpm {
    NTSTATUS SpoofVars() {
        GUID g[] = { {0xeaec226f,0xc9a3,0x477a,{0xa8,0x26,0xdd,0xc7,0x16,0xcd,0xc0,0xe3}}, {0x1b463f9c,0x803c,0x49e4,{0xb4,0x68,0x28,0x68,0x90,0x84,0x79,0xb2}} };
        const wchar_t* v[] = { L"UnlockIDCopy", L"OfflineUniqueIDEKPubCRC", L"OfflineUniqueIDEKPub" };
        for (int i = 0; i < 3; i++) {
            UNICODE_STRING us; RtlInitUnicodeString(&us, v[i]);
            for (int k = 0; k < 2; k++) {
                UCHAR b[1024]; ULONG l = sizeof(b), a = 0;
                if (NT_SUCCESS(ExGetFirmwareEnvironmentVariable(&us, &g[k], b, &l, &a)) && l) {
                    Utils::RandomBuffer(b, l);
                    ExSetFirmwareEnvironmentVariable(&us, &g[k], b, l, a | 1);
                    break;
                }
            }
        }
        return 0;
    }
}
