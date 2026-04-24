#include "bedrock/bedrock_string.h"
#include "os/os.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

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

static String8 os_path(Arena *a, String8 path) {
        if (string8_is_null_term(path)) {
                return path;
        }
        String8 res = string8_allocate(a, path.length + 1);

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
        }
        file->handle = open(path.data, flags);

        if (file->handle == -1) {
                switch (errno) {
                default:
                        return OS_FileCode_UnknownError;
                case ENOENT:
                        return OS_FileCode_DoesNotExist;
                case EACCES:
                        return OS_FileCode_AccessDenied;
                }
        }
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

static OS_FileCode os_file_read(OSFile file, void *ptr, u64 size) {
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

static OS_FileCode os_file_write(OSFile file, void *ptr, u64 size) {
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
