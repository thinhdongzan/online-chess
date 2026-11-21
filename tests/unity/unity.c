#include "unity.h"

struct UnityStorage Unity;

static void UnityPrintHeader(uint32_t line)
{
    printf("%s:%u:%s:",
           Unity.TestFile ? Unity.TestFile : "unknown",
           (unsigned int)line,
           Unity.CurrentTestName ? Unity.CurrentTestName : "test");
}

static void UnityPrintMessage(const char *msg)
{
    if (msg != NULL && msg[0] != '\0') {
        printf(" (%s)", msg);
    }
}

void UnityBegin(const char *filename)
{
    Unity.TestFile = filename;
    Unity.CurrentTestName = NULL;
    Unity.CurrentTestLineNumber = 0;
    Unity.NumberOfTests = 0;
    Unity.TestFailures = 0;
    Unity.TestIgnores = 0;
    Unity.CurrentTestFailed = 0;
    Unity.CurrentTestIgnored = 0;
}

int UnityEnd(void)
{
    printf("\n-----------------------\n");
    printf("%u Tests %u Failures %u Ignored\n",
           (unsigned int)Unity.NumberOfTests,
           (unsigned int)Unity.TestFailures,
           (unsigned int)Unity.TestIgnores);
    if (Unity.TestFailures == 0U) {
        printf("OK\n");
    } else {
        printf("FAIL\n");
    }
    fflush(stdout);
    return (int)Unity.TestFailures;
}

void UnityDefaultTestRun(UnityTestFunction func, const char *func_name, int func_line_num)
{
    Unity.CurrentTestName = func_name;
    Unity.CurrentTestLineNumber = (uint32_t)func_line_num;
    Unity.NumberOfTests++;
    Unity.CurrentTestFailed = 0;
    Unity.CurrentTestIgnored = 0;

    if (setjmp(Unity.AbortFrame) == 0) {
        setUp();
        func();
    }

    if (setjmp(Unity.AbortFrame) == 0) {
        tearDown();
    }

    if (Unity.CurrentTestFailed) {
        Unity.TestFailures++;
    } else if (Unity.CurrentTestIgnored) {
        Unity.TestIgnores++;
    } else {
        UnityPrintHeader(Unity.CurrentTestLineNumber);
        printf("PASS\n");
    }
    fflush(stdout);
}

void UnityFail(const char *message, uint32_t line)
{
    Unity.CurrentTestFailed = 1;
    UnityPrintHeader(line);
    printf("FAIL");
    if (message != NULL && message[0] != '\0') {
        printf(": %s", message);
    }
    printf("\n");
    longjmp(Unity.AbortFrame, 1);
}

void UnityIgnore(const char *message, uint32_t line)
{
    Unity.CurrentTestIgnored = 1;
    UnityPrintHeader(line);
    printf("IGNORE");
    if (message != NULL && message[0] != '\0') {
        printf(": %s", message);
    }
    printf("\n");
    longjmp(Unity.AbortFrame, 1);
}

void UnityAssertEqualNumber(int64_t expected,
                            int64_t actual,
                            const char *msg,
                            uint32_t line)
{
    if (expected != actual) {
        Unity.CurrentTestFailed = 1;
        UnityPrintHeader(line);
        printf("FAIL: Expected %lld Was %lld",
               (long long)expected,
               (long long)actual);
        UnityPrintMessage(msg);
        printf("\n");
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertEqualUnsignedNumber(uint64_t expected,
                                    uint64_t actual,
                                    const char *msg,
                                    uint32_t line)
{
    if (expected != actual) {
        Unity.CurrentTestFailed = 1;
        UnityPrintHeader(line);
        printf("FAIL: Expected %llu Was %llu",
               (unsigned long long)expected,
               (unsigned long long)actual);
        UnityPrintMessage(msg);
        printf("\n");
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertNumbersWithin(int64_t delta,
                              int64_t expected,
                              int64_t actual,
                              const char *msg,
                              uint32_t line)
{
    int64_t diff = (actual >= expected) ? (actual - expected) : (expected - actual);
    if (delta < 0) {
        delta = -delta;
    }
    if (diff > delta) {
        Unity.CurrentTestFailed = 1;
        UnityPrintHeader(line);
        printf("FAIL: Values Not Within Delta %lld (Expected %lld Was %lld)",
               (long long)delta,
               (long long)expected,
               (long long)actual);
        UnityPrintMessage(msg);
        printf("\n");
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertGreaterOrLess(int64_t threshold,
                              int64_t actual,
                              int cmp_mode,
                              const char *msg,
                              uint32_t line)
{
    int ok = 0;
    const char *desc = "";
    if (cmp_mode == 1) {
        ok = (actual > threshold);
        desc = "Greater Than";
    } else if (cmp_mode == 2) {
        ok = (actual >= threshold);
        desc = "Greater Or Equal To";
    } else if (cmp_mode == -1) {
        ok = (actual < threshold);
        desc = "Less Than";
    } else if (cmp_mode == -2) {
        ok = (actual <= threshold);
        desc = "Less Or Equal To";
    }

    if (!ok) {
        Unity.CurrentTestFailed = 1;
        UnityPrintHeader(line);
        printf("FAIL: Expected %lld To Be %s %lld",
               (long long)actual,
               desc,
               (long long)threshold);
        UnityPrintMessage(msg);
        printf("\n");
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertEqualString(const char *expected,
                            const char *actual,
                            const char *msg,
                            uint32_t line)
{
    if (expected == actual) {
        return;
    }
    if (expected == NULL || actual == NULL || strcmp(expected, actual) != 0) {
        Unity.CurrentTestFailed = 1;
        UnityPrintHeader(line);
        printf("FAIL: Expected '%s' Was '%s'",
               expected ? expected : "NULL",
               actual ? actual : "NULL");
        UnityPrintMessage(msg);
        printf("\n");
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertEqualStringLen(const char *expected,
                               const char *actual,
                               size_t length,
                               const char *msg,
                               uint32_t line)
{
    if (length == 0U || expected == actual) {
        return;
    }
    if (expected == NULL || actual == NULL || strncmp(expected, actual, length) != 0) {
        Unity.CurrentTestFailed = 1;
        UnityPrintHeader(line);
        printf("FAIL: Strings Differ Within First %zu Bytes", length);
        UnityPrintMessage(msg);
        printf("\n");
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertEqualMemory(const void *expected,
                            const void *actual,
                            size_t length,
                            const char *msg,
                            uint32_t line)
{
    if (length == 0U || expected == actual) {
        return;
    }
    if (expected == NULL || actual == NULL || memcmp(expected, actual, length) != 0) {
        Unity.CurrentTestFailed = 1;
        UnityPrintHeader(line);
        printf("FAIL: Memory Comparison Failed (%zu bytes)", length);
        UnityPrintMessage(msg);
        printf("\n");
        longjmp(Unity.AbortFrame, 1);
    }
}
