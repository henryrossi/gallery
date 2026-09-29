#include "os/os.h"

#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static void *os_commit(u64 size) {
        void *res = mmap(NULL, size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANON, -1, 0);
        if (res == MAP_FAILED) {
                perror("ERROR: Memory allocation failed");
        }
        return res;
}

static void os_release(void *ptr, u64 size) {
        // NOTE: hr: There are a couple scenarios when munmap might fail.
        munmap(ptr, size);
}

static String8 os_path(Arena *a, String8 path) {
        if (string8_is_null_term(path)) {
                return path;
        }
        String8 res = string8_allocate_a(a, path.length + 1);

        memcpy(res.data, path.data, path.length);
        string8_set(res, res.length - 1, '\0');

        return res;
}

static OS_FileCode os_open_file(String8 path, OSFile *file, OS_FileAccess acc) {
        if (!file) {
                return OS_FileCode_NullArgument;
        }

        file->handle = -1;
        if (!string8_is_null_term(path)) {
                return OS_FileCode_UntermPathString;
        }

        s32 flags = 0;
        switch (acc) {
        case OS_FileAccess_Read:
                flags = O_RDONLY;
                break;
        case OS_FileAccess_Write:
                flags = O_WRONLY;
                break;
        case OS_FileAccess_ReadWrite:
                flags = O_RDWR;
                break;
        case OS_FileAccess_Create:
                flags = O_CREAT | O_RDWR;
                break;
        }
        file->handle = open((char *)path.data, flags);

        OS_FileCode res = OS_FileCode_Success;
        if (file->handle == -1) {
                switch (errno) {
                default:
                        res = OS_FileCode_UnknownError;
                case ENOENT:
                        res = OS_FileCode_DoesNotExist;
                case EACCES:
                        res = OS_FileCode_AccessDenied;
                }
        }
        return res;
}

static OSFileInfo os_file_info(OSFile file) {
        OSFileInfo info = { 0 };

        struct stat s;
        fstat(file.handle, &s);

        info.size = s.st_size;
        return info;
}

static OS_FileCode os_move_file_pos(OSFile file, s64 offset) {
        u64 pos = lseek(file.handle, offset, SEEK_CUR);
        if (pos == -1) {
                switch (errno) {
                default:
                        return OS_FileCode_UnknownError;
                case EBADF:
                        return OS_FileCode_BadFileHandle;
                }
        }
        return OS_FileCode_Success;
}

static OS_FileCode os_read_file(OSFile file, void *ptr, u64 size) {
        if (!ptr) {
                return OS_FileCode_NullArgument;
        }

        u32 res = read(file.handle, ptr, size);
        if (res == -1) {
                switch (errno) {
                default:
                        return OS_FileCode_UnknownError;
                case EBADF:
                        return OS_FileCode_BadFileHandle;
                }
        }
        return OS_FileCode_Success;
}

static OS_FileCode os_write_file(OSFile file, void *ptr, u64 size) {
        if (!ptr) {
                return OS_FileCode_NullArgument;
        }

        u32 res = write(file.handle, ptr, size);
        if (res == -1) {
                switch (errno) {
                default:
                        return OS_FileCode_UnknownError;
                case EBADF:
                        return OS_FileCode_BadFileHandle;
                }
        }
        return OS_FileCode_Success;
}

static void os_close_file(OSFile file) {
        close(file.handle);
}

static OSFile os_stderr(void) {
        OSFile res = { STDERR_FILENO };
        return res;
}

static Stacktrace os_get_stack_trace(Arena *arena) {
        Stacktrace res = { 0 };
        u64 max = 48;
        res.addrs = arena_alloc(arena, sizeof(void *) * max);
        res.count = backtrace(res.addrs, max);
        arena_pop(arena, sizeof(void *) * (max - res.count));
        return res;
}

static void os_write_stack_trace(OSFile file, Stacktrace st) {
        backtrace_symbols_fd(st.addrs, st.count, file.handle);
}
