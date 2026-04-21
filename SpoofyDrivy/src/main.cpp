#include "ndis.h"
#include "smbios.h"
#include "monitor.h"
#include "tcpip.h"
#include "usb.h"
#include "tpm.h"

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT, PUNICODE_STRING)
{
    Smbios::ChangeSmbiosSerials();
    Tpm::SpoofVars();
    Ndis::SpoofMAC();
    Tcpip::SpoofArp();
    Monitor::SpoofMonitors();
    Usb::SpoofSerials();
    return STATUS_SUCCESS;
}
