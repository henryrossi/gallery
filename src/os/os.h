#ifndef OS_INC_H
#define OS_INC_H

#include "bedrock/bedrock_arena.h"
#include "bedrock/bedrock_core.h"
#include "bedrock/bedrock_string.h"

typedef struct {
        u64 pageFaults;
        u64 memUsed;
} OSRUsage;

typedef struct {
        u64 handle;
} OSFile;

typedef enum {
        OS_FileAccess_Read = 1 << 0,
        OS_FileAccess_Write = 1 << 1,
        OS_FileAccess_ReadWrite = OS_FileAccess_Read | OS_FileAccess_Write,
        OS_FileAccess_Create = 1 << 2,
} OS_FileAccess;

typedef struct {
        u64 size;
} OSFileInfo;

typedef enum {
        OS_FileCode_Success = 1,

        OS_FileCode_UnknownError = -1,
        OS_FileCode_DoesNotExist = -2,
        OS_FileCode_AccessDenied = -3,
        OS_FileCode_BadFileHandle = -4,
        OS_FileCode_NullArgument = -5,
        OS_FileCode_UntermPathString = -6,
} OS_FileCode;

static void os_abort(s32 exitCode);

static void *os_commit(u64 size);
static void os_release(void *ptr, u64 size);

static b32 os_rusage(OSRUsage *usage);

//  NOTE: hr: returns a c style, null terminated string
static String8 os_path(Arena *a, String8 path);

//  NOTE: hr: path needs to be a c style, null terminated string
static OS_FileCode os_open_file(String8 path, OSFile *file, OS_FileAccess acc);
static OSFileInfo os_file_info(OSFile file);
static OS_FileCode os_move_file_pos(OSFile file, s64 offset);
static OS_FileCode os_read_file(OSFile file, void *ptr, u64 size);
static OS_FileCode os_write_file(OSFile file, void *ptr, u64 size);
static void os_close_file(OSFile file);

static OSFile os_stdin(void);
static OSFile os_stdout(void);
static OSFile os_stderr(void);

typedef struct {
        void *addrs;
        u64 count;
} Stacktrace;

static Stacktrace os_get_stack_trace(Arena *arena);
static void os_write_stack_trace(OSFile file, Stacktrace st);

#if __MACH__
#include "os/mach/os_mach.h"
#elif _WIN32
#include "os/windows/os_windows.h"
#elif __linux__
#error OS core layer not implemented for Linux.
#else
#error OS core layer not implemented for this operating system.
#endif

#endif // OS_INC_H
