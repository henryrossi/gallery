#ifndef BEDROCK_LOGS_H
#define BEDROCK_LOGS_H

#include "bedrock/bedrock_core.h"
#include "bedrock/bedrock_string.h"
#include "os/os.h"

// hr: see "The Easiest Way to Handle Errors Is To Not Have Them" Ryan Fleury -
// Error Information Side-Channels.

typedef struct Message Message;
struct Message {
        Stacktrace callstack;
        String8 str;
        Message *next;
        Message *prev;
};

typedef struct {
        Message *first;
        Message *last;
        u64 count;
} MessageList;

static void log_message(String8 str);
#ifdef STRING8F
static void log_messagef(char *fmt, ...);
#endif

static void log_dump(OSFile file, u64 limit);
static void log_clear_messages(void);

#endif // BEDROCK_LOGS_H
