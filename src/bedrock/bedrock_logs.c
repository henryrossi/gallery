#define STRING8F

#include "bedrock/bedrock_logs.h"
#include "bedrock/bedrock_arena.h"

__thread Arena *log_arena = 0;
__thread MessageList log_messages = { 0 };

static void log_message(String8 str) {
        if (!log_arena) {
                log_arena = make_arena(kb(48));
        }

        Message *msg = arena_alloc(log_arena, sizeof(*msg));
        msg->str = str;
        msg->callstack = os_get_stack_trace(log_arena);
        DLLPushBack(log_messages.first, log_messages.last, msg);
        log_messages.count++;
}

static void log_messagef(char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        String8 str = string8fv(log_arena, fmt, args);
        va_end(args);
        log_message(str);
}

static void log_dump(OSFile file, u64 limit) {
        Message *msg = log_messages.first;
        if (limit && limit < log_messages.count) {
                msg = log_messages.last;
        }
        for (u64 i = 0; i < limit && msg->prev; i++) {
                msg = msg->prev;
        }
        for (; msg; msg = msg->next) {
                os_write_file(file, msg->str.data, msg->str.length);
                os_write_stack_trace(file, msg->callstack);
        }
}

static void log_clear_messages(void) {
        log_messages.first = 0;
        log_messages.last = 0;
        log_messages.count = 0;
        arena_reset(log_arena);
}
