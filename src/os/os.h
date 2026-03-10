#ifndef OS_INC_H
#define OS_INC_H

#include <stdlib.h>

typedef struct {
        u64 size;
} OSFileInfo;

static void os_abort(s32 exitCode);

static void *os_commit(u64 size);
static void os_release(void *ptr, u64 size);

// NOTE: hr: it'll probably be better to use a fd rather than a filepath
static OSFileInfo os_file_info(const char *filename);

#if __MACH__
#include "os/mach/os_mach.h"
#elif _WIN32
#include "os/windows/os_windows.h"
#elif __linux__
#error OS core layer not implemented for Linux.
#else
#error OS core layer not implemented for this operating system.
#endif

#endif // OS_INC_H
