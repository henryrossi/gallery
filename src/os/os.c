#include "os/os.h"

#include <stdlib.h>

#if __MACH__
#include "mach/os_mach.c"
#elif _WIN32
#include "windows/os_windows.c"
#elif __linux__
#error OS core layer not implemented for Linux.
#else
#error OS core layer not implemented for this operating system.
#endif

static void os_abort(s32 exitCode) {
        exit(exitCode);
}
