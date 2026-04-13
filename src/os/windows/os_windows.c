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
