#include "testing/test.h"
#include "bedrock/bedrock_core.h"

#include <time.h>

typedef struct {
        u64 input;
        String8 name;
} TestingInput;

#define TESTING_INPUT_MAX 64

static u64 testing_last_error_count = 0;
static TestingInput testing_last_error[TESTING_INPUT_MAX] = { 0 };

static void testing_push_error_input(u64 input, String8 name) {
        TestingInput in = { input, name };
        testing_last_error[testing_last_error_count++] = in;
}

#include "os/os.c"

#include "bedrock/bedrock_inc.c"
#include "bedrock/tests.c"

typedef b32 (*TestingCaseFunc)(u64, u64);
typedef struct {
        String8 name;
        TestingCaseFunc test;
} TestingCase;

static TestingCase testing_cases[] = {
        { ccstring8_lit("Arena"), &test_arena },
};

int main(int argc, char *argv[]) {
        u64 seed = 0, reps = 0;
        if (argc > 1) {
                reps = strtoll(argv[1], 0, 10);
        }
        if (argc > 2) {
                seed = strtoll(argv[2], 0, 10);
        }
        if (!reps) {
                reps = 3000000;
        }
        if (!seed) {
                time((s64 *)&seed);
        }

        printf("--- starting testing, seed: %llu ----------\n", seed);

        u64 numCases = array_count(testing_cases);
        u64 passed = 0;
        for (u64 i = 0; i < numCases; i++) {
                TestingCase c = testing_cases[i];
                b32 res = c.test(seed, reps);
                if (res) {
                        passed++;
                } else {
                        printf("    X test failed - ");
                        print_string8(c.name);
                        printf(" - input { ");
                        for (u32 i = 0; i < testing_last_error_count; i++) {
                                TestingInput in = testing_last_error[i];
                                print_string8(in.name);
                                printf(": %llu, ", in.input);
                        }
                        printf("}\n");
                }
        }

        printf("[%llu/%llu] tests passing\n", passed, numCases);

        return 0;
}
