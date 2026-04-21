#pragma once
#include <ntifs.h>

#define LOG(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, (fmt), ##__VA_ARGS__)