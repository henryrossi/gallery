#include "bedrock/bedrock_string.h"
#include "os/os.h"

#include <errhandlingapi.h>
#include <windows.h>
#include <winnt.h>

static void *os_commit(u64 size) {
        void *res = VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
        if (!res) {
                u32 err = GetLastError();
                // NOTE: hr: use FormatMessage() to get error string
        }
        return res;
}

static void os_release(void *ptr, u64 size) {
        // NOTE: hr: There are a couple scenarios when free might fail. Is ptr
        //           valid?
        VirtualFree(ptr, size, MEM_DECOMMIT);
}

static String8 os_path(Arena *a, String8 path) {
        // hr: for now replaces '/' with '\\'
        String8 res = { 0 };
        u32 extra = string8_is_null_term(path) ? 0 : 1;
        for (u64 i = 0; i < path.length; i++) {
                if (string8_at(path, i) == '/') {
                        extra++;
                }
        }
        res = string8_allocate(a, path.length + extra);

        for (u64 i = 0, j = 0; i < path.length; i++, j++) {
                if (string8_at(path, i) == '/') {
                        string8_set(res, j, '\\');
                        string8_set(res, ++j, '\\');
                } else {
                        string8_set(res, j, string8_at(path, i));
                }
        }
        string8_set(res, res.length - 1, '\0');

        return res;
}

static OS_FileCode os_open_file(String8 path, OSFile *file, OS_FileAccess acc) {
        if (!file) {
                return OS_FileCode_NullArgument;
        }
        file->handle = -1;

        if (string8_is_null_term(path)) {
                return OS_FileCode_UntermPathString;
        }

        u32 access = 0;
        switch (acc) {
        case OS_FileAccess_Read:
                access = GENERIC_READ;
                break;
        case OS_FileAccess_Write:
                access = GENERIC_WRITE;
                break;
        case OS_FileAccess_ReadWrite:
                access = GENERIC_READ | GENERIC_WRITE;
                break;
        }

        HANDLE hFile = CreateFileA((char *)path.data, access, 0, 0,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if (hFile == INVALID_HANDLE_VALUE) {
                u32 err = GetLastError();
                switch (err) {
                case ERROR_ACCESS_DENIED:
                        return OS_FileCode_AccessDenied;
                case ERROR_FILE_NOT_FOUND:
                        return OS_FileCode_DoesNotExist;
                default:
                        return OS_FileCode_UnknownError;
                }
        }

        file->handle = (u64)hFile;
        return OS_FileCode_Success;
}

static OSFileInfo os_file_info(OSFile file) {
        OSFileInfo info = { 0 };
        HANDLE hFile = (HANDLE)file.handle;
        if (hFile == INVALID_HANDLE_VALUE) {
                return info;
        }
        info.size = GetFileSize(hFile, 0);
        return info;
}

static OS_FileCode os_move_file_pos(OSFile file, s64 offset) {
        HANDLE hFile = (HANDLE)file.handle;
        u32 res
            = SetFilePointerEx(hFile, (LARGE_INTEGER)offset, 0, FILE_CURRENT);
        if (res <= 0) {
                return 0;
        }
        return 1;
}

static OS_FileCode os_file_read(OSFile file, void *ptr, u64 size) {
        if (!ptr) {
                return OS_FileCode_NullArgument;
        }

        HANDLE hFile = (HANDLE)file.handle;
        DWORD read = 0;
        b32 res = ReadFile(hFile, ptr, size, &read, 0);

        if (!res) {
                return OS_FileCode_UnknownError;
        }

        return OS_FileCode_Success;
}

static OS_FileCode os_file_write(OSFile file, void *ptr, u64 size) {
        if (!ptr) {
                return OS_FileCode_NullArgument;
        }

        HANDLE hFile = (HANDLE)file.handle;
        DWORD written = 0;
        b32 res = ReadFile(hFile, ptr, size, &written, 0);

        if (!res) {
                return OS_FileCode_UnknownError;
        }

        return OS_FileCode_Success;
}

static void os_close_file(OSFile file) {
        HANDLE hFile = (HANDLE)file.handle;
        CloseHandle(hFile);
}
