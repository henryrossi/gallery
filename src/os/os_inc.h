#ifndef OS_INC_H
#define OS_INC_H

#if __MACH__
#include "os/mach/os_mach.h"
#elif _WIN32
# error OS core layer not implemented for Windows.
#elif __linux__
# error OS core layer not implemented for Linux.
#else
# error OS core layer not implemented for this operating system.
#endif

#endif // OS_INC_H
