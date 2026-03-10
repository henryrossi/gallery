#include <sys/mman.h>
#include <sys/stat.h>

static void *os_commit(u64 size) {
        void *res = mmap(NULL, size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANON, -1, 0);
        if (res == MAP_FAILED)
                perror("ERROR: Memory allocation failed");
        return res;
}

static void os_release(void *ptr, u64 size) {
        // NOTE: hr: There are a couple scenarios when munmap might fail.
        munmap(ptr, size);
}

static OSFileInfo os_file_info(const char *filename) {
        OSFileInfo info = { 0 };
        struct stat s;
        stat(filename, &s); // hr: fstat for fd

        info.size = s.st_size;
        return info;
}
