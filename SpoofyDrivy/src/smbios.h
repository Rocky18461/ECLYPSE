#pragma once
#include <ntifs.h>
#pragma pack(push, 1)
struct SMBIOS_HEADER { UCHAR Type, Length; USHORT Handle; };
struct SMBIOS_TYPE1 { SMBIOS_HEADER Header; UCHAR M, P, V, SerialNumber; UCHAR UUID[16]; };
struct SMBIOS_TYPE2 { SMBIOS_HEADER Header; UCHAR M, P, V, SerialNumber; };
struct SMBIOS_TYPE3 { SMBIOS_HEADER Header; UCHAR M, T, V, SerialNumber; };
struct SMBIOS_TYPE4 { SMBIOS_HEADER Header; UCHAR S, T, F, M; ULONGLONG ProcessorId; UCHAR V; USHORT E, Ma, C; UCHAR St, U; USHORT L1, L2, L3; UCHAR SerialNumber; };
#pragma pack(pop)
namespace Smbios { NTSTATUS ChangeSmbiosSerials(); }
