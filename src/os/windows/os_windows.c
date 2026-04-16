#include "os/os.h"

#include <windows.h>

static void *os_commit(u64 size) {
        void *res = VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
        if (!res) {
                u32 err = GetLastError();
                // NOTE: hr: use FormatMessage() to get error string
        }
        return res;
}

static void os_release(void *ptr, u64 size) {
        // NOTE: hr: There are a couple scenarios when free might fail.
        VirtualFree(ptr, size, MEM_DECOMMIT);
}

static String8 os_path(Arena *a, String8 path) {
        // hr: for now replaces '/' with '\\'
        String8 res = { 0 };
        u32 extra = 1;
        for (u64 i = 0; i < path.length; i++) {
                if (string8_at(path, i) == '/') {
                        extra++;
                }
        }
        res = string8_allocate(a, path.length + extra);

        for (u64 i = 0, j = 0; i < path.length; i++, j++) {
                if (string8_at(path, i) == '/') {
                        res.data[j] = '\\';
                        res.data[++j] = '\\';
                } else {
                        res.data[j] = string8_at(path, i);
                }
        }
        res.data[res.length - 1] = 0;

        return res;
}

static OSFileInfo os_file_info(const char *filename) {
        OSFileInfo info = { 0 };
        HANDLE hFile = CreateFileA(filename, GENERIC_READ, 0, 0, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, 0);
        if (hFile == INVALID_HANDLE_VALUE) {
                return info;
        }
        info.size = GetFileSize(hFile, 0);
        CloseHandle(hFile);
        return info;
}
