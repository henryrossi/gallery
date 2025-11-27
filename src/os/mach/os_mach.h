#ifndef OS_MACH_H
#define OS_MACH_H

#define OS_PAGESIZE 0x4000

static void *os_reserve(u64 size);
static void os_release(void *ptr, u64 size);

#endif // OS_MACH_H
