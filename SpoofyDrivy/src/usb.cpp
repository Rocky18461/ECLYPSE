#include "usb.h"
#include "utils.h"
#include "log.h"

extern "C" NTSTATUS ObReferenceObjectByName(PUNICODE_STRING, ULONG, PACCESS_STATE, ACCESS_MASK, POBJECT_TYPE, KPROCESSOR_MODE, PVOID, PVOID*);
extern "C" POBJECT_TYPE* IoDriverObjectType;

namespace Usb {
    NTSTATUS SpoofSerials() {
        UNICODE_STRING drvn; RtlInitUnicodeString(&drvn, L"\\Driver\\USBSTOR");
        PDRIVER_OBJECT drv = nullptr;
        if (!NT_SUCCESS(ObReferenceObjectByName(&drvn, 64, 0, 0, *IoDriverObjectType, KernelMode, 0, (PVOID*)&drv))) return 0;
        LARGE_INTEGER t; KeQueryTickCount(&t); ULONG s = t.LowPart;
        for (PDEVICE_OBJECT dev = drv->DeviceObject; dev; dev = dev->NextDevice) {
            PUCHAR ext = (PUCHAR)dev->DeviceExtension;
            if (ext && MmIsAddressValid(ext)) {
                ULONG sig = *(PULONG)ext;
                if (sig == 0x214F4450 || sig == 0x214F4446) {
                    ULONG len = *(PULONG)(ext + 0x40);
                    PUCHAR ser = ext + 0x6C;
                    if (len > 0 && len < 256 && MmIsAddressValid(ser)) {
                        for (ULONG i = 0; i < len; i++) if (ser[i] >= '0' && ser[i] <= 'z') ser[i] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[RtlRandomEx(&s) % 36];
                    }
                }
            }
        }
        ObDereferenceObject(drv);
        return 0;
    }
}
