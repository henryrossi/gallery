#include "bedrock/bedrock_string.h"
#include "os/os.h"

#include <windows.h>

#include <psapi.h>

static void *os_commit(u64 size) {
        void *res = VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
        if (!res) {
                // NOTE: hr: messy :/
                u32 err = GetLastError();
                LPSTR messageBuffer = 0;

                size_t size = FormatMessageA(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                        | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPSTR)&messageBuffer, 0, NULL);

                String8 str = { .data = (u8 *)messageBuffer, .length = size };
                print_string8(str);

                LocalFree(messageBuffer);

                OSRUsage use = { 0 };
                os_rusage(&use);
                printf("Total process memory used: %llu\n", use.memUsed);
        }
        return res;
}

static void os_release(void *ptr, u64 size) {
        // NOTE: hr: There are a couple scenarios when free might fail. Is ptr
        //           valid?
        VirtualFree(ptr, size, MEM_DECOMMIT);
}

static b32 os_rusage(OSRUsage *usage) {
        HANDLE hProcess;
        PROCESS_MEMORY_COUNTERS pmc;

        u64 pid = GetCurrentProcessId();
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                               FALSE, pid);
        if (!hProcess) {
                return 0;
        }

        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                usage->pageFaults = pmc.PageFaultCount;
                usage->memUsed = pmc.WorkingSetSize;
                CloseHandle(hProcess);
                return 1;
        }

        CloseHandle(hProcess);
        return 0;
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
        u32 creation = OPEN_EXISTING;
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
        case OS_FileAccess_Create:
                access = GENERIC_READ | GENERIC_WRITE;
                creation = CREATE_ALWAYS;
                break;
        }

        HANDLE hFile = CreateFileA((char *)path.data, access, 0, 0, creation,
                                   FILE_ATTRIBUTE_NORMAL, 0);
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
