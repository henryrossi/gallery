#include <sys/mman.h>

static void *os_reserve(u64 size) {
        void *res = mmap(NULL, size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANON, -1, 0);
        if (res == MAP_FAILED)
                perror(0);
        return res;
}

static void os_release(void *ptr, u64 size) {
        // hr: There are a couple scenarios when munmap might fail.
        munmap(ptr, size);
}
