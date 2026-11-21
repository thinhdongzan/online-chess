#ifndef UNITY_FRAMEWORK_H
#define UNITY_FRAMEWORK_H

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*UnityTestFunction)(void);

struct UnityStorage {
    const char *TestFile;
    const char *CurrentTestName;
    uint32_t CurrentTestLineNumber;
    uint32_t NumberOfTests;
    uint32_t TestFailures;
    uint32_t TestIgnores;
    uint8_t CurrentTestFailed;
    uint8_t CurrentTestIgnored;
    jmp_buf AbortFrame;
};

extern struct UnityStorage Unity;

/* Test lifecycle hooks defined by each test file */
void setUp(void);
void tearDown(void);

/* Core framework control functions */
void UnityBegin(const char *filename);
int UnityEnd(void);
void UnityDefaultTestRun(UnityTestFunction func, const char *func_name, int func_line_num);

/* Failure and ignore reporting helpers */
void UnityFail(const char *message, uint32_t line);
void UnityIgnore(const char *message, uint32_t line);

void UnityAssertEqualNumber(int64_t expected,
                            int64_t actual,
                            const char *msg,
                            uint32_t line);
void UnityAssertEqualUnsignedNumber(uint64_t expected,
                                    uint64_t actual,
                                    const char *msg,
                                    uint32_t line);
void UnityAssertNumbersWithin(int64_t delta,
                              int64_t expected,
                              int64_t actual,
                              const char *msg,
                              uint32_t line);
void UnityAssertGreaterOrLess(int64_t threshold,
                              int64_t actual,
                              int cmp_mode,
                              const char *msg,
                              uint32_t line);
void UnityAssertEqualString(const char *expected,
                            const char *actual,
                            const char *msg,
                            uint32_t line);
void UnityAssertEqualStringLen(const char *expected,
                               const char *actual,
                               size_t length,
                               const char *msg,
                               uint32_t line);
void UnityAssertEqualMemory(const void *expected,
                            const void *actual,
                            size_t length,
                            const char *msg,
                            uint32_t line);

/* Runner macros */
#define UNITY_BEGIN() UnityBegin(__FILE__)
#define UNITY_END()   UnityEnd()

#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)

/* Flow control assertions */
#define TEST_PASS()                                                            \
    do {                                                                       \
        longjmp(Unity.AbortFrame, 1);                                          \
    } while (0)

#define TEST_FAIL_MESSAGE(message)                                             \
    do {                                                                       \
        UnityFail((message), (uint32_t)__LINE__);                              \
    } while (0)

#define TEST_FAIL() TEST_FAIL_MESSAGE(NULL)

#define TEST_IGNORE_MESSAGE(message)                                           \
    do {                                                                       \
        UnityIgnore((message), (uint32_t)__LINE__);                            \
    } while (0)

#define TEST_IGNORE() TEST_IGNORE_MESSAGE(NULL)

/* Boolean and pointer assertions */
#define TEST_ASSERT_MESSAGE(condition, message)                                \
    do {                                                                       \
        if (!(condition)) {                                                    \
            UnityFail((message), (uint32_t)__LINE__);                          \
        }                                                                      \
    } while (0)

#define TEST_ASSERT(condition)                                                 \
    TEST_ASSERT_MESSAGE((condition), "Expression Evaluated To FALSE")

#define TEST_ASSERT_TRUE_MESSAGE(condition, message)                           \
    TEST_ASSERT_MESSAGE((condition), (message))

#define TEST_ASSERT_TRUE(condition)                                            \
    TEST_ASSERT_MESSAGE((condition), "Expected TRUE Was FALSE")

#define TEST_ASSERT_FALSE_MESSAGE(condition, message)                          \
    TEST_ASSERT_MESSAGE(!(condition), (message))

#define TEST_ASSERT_FALSE(condition)                                           \
    TEST_ASSERT_MESSAGE(!(condition), "Expected FALSE Was TRUE")

#define TEST_ASSERT_NULL_MESSAGE(pointer, message)                             \
    TEST_ASSERT_MESSAGE(((pointer) == NULL), (message))

#define TEST_ASSERT_NULL(pointer)                                              \
    TEST_ASSERT_NULL_MESSAGE((pointer), "Expected NULL")

#define TEST_ASSERT_NOT_NULL_MESSAGE(pointer, message)                         \
    TEST_ASSERT_MESSAGE(((pointer) != NULL), (message))

#define TEST_ASSERT_NOT_NULL(pointer)                                          \
    TEST_ASSERT_NOT_NULL_MESSAGE((pointer), "Expected Non-NULL")

/* Signed integer assertions */
#define TEST_ASSERT_EQUAL_INT_MESSAGE(expected, actual, message)               \
    UnityAssertEqualNumber((int64_t)(expected),                                \
                           (int64_t)(actual),                                  \
                           (message),                                          \
                           (uint32_t)__LINE__)

#define TEST_ASSERT_EQUAL_INT(expected, actual)                                \
    TEST_ASSERT_EQUAL_INT_MESSAGE((expected), (actual), NULL)

#define TEST_ASSERT_EQUAL_INT8(expected, actual)                               \
    TEST_ASSERT_EQUAL_INT_MESSAGE((int8_t)(expected), (int8_t)(actual), NULL)

#define TEST_ASSERT_EQUAL_INT16(expected, actual)                              \
    TEST_ASSERT_EQUAL_INT_MESSAGE((int16_t)(expected), (int16_t)(actual), NULL)

#define TEST_ASSERT_EQUAL_INT32(expected, actual)                              \
    TEST_ASSERT_EQUAL_INT_MESSAGE((int32_t)(expected), (int32_t)(actual), NULL)

#define TEST_ASSERT_EQUAL_INT64(expected, actual)                              \
    TEST_ASSERT_EQUAL_INT_MESSAGE((int64_t)(expected), (int64_t)(actual), NULL)

#define TEST_ASSERT_EQUAL(expected, actual)                                    \
    TEST_ASSERT_EQUAL_INT((expected), (actual))

#define TEST_ASSERT_EQUAL_MESSAGE(expected, actual, message)                   \
    TEST_ASSERT_EQUAL_INT_MESSAGE((expected), (actual), (message))

#define TEST_ASSERT_NOT_EQUAL_MESSAGE(expected, actual, message)               \
    TEST_ASSERT_MESSAGE(((expected) != (actual)), (message))

#define TEST_ASSERT_NOT_EQUAL(expected, actual)                                \
    TEST_ASSERT_NOT_EQUAL_MESSAGE((expected), (actual), "Expected Not-Equal")

#define TEST_ASSERT_EQUAL_CHAR(expected, actual)                               \
    TEST_ASSERT_EQUAL_INT_MESSAGE((char)(expected), (char)(actual), NULL)

/* Unsigned integer & hex assertions */
#define TEST_ASSERT_EQUAL_UINT_MESSAGE(expected, actual, message)              \
    UnityAssertEqualUnsignedNumber((uint64_t)(expected),                       \
                                   (uint64_t)(actual),                         \
                                   (message),                                  \
                                   (uint32_t)__LINE__)

#define TEST_ASSERT_EQUAL_UINT(expected, actual)                               \
    TEST_ASSERT_EQUAL_UINT_MESSAGE((expected), (actual), NULL)

#define TEST_ASSERT_EQUAL_UINT8(expected, actual)                              \
    TEST_ASSERT_EQUAL_UINT_MESSAGE((uint8_t)(expected), (uint8_t)(actual), NULL)

#define TEST_ASSERT_EQUAL_UINT16(expected, actual)                             \
    TEST_ASSERT_EQUAL_UINT_MESSAGE((uint16_t)(expected), (uint16_t)(actual), NULL)

#define TEST_ASSERT_EQUAL_UINT32(expected, actual)                             \
    TEST_ASSERT_EQUAL_UINT_MESSAGE((uint32_t)(expected), (uint32_t)(actual), NULL)

#define TEST_ASSERT_EQUAL_UINT64(expected, actual)                             \
    TEST_ASSERT_EQUAL_UINT_MESSAGE((uint64_t)(expected), (uint64_t)(actual), NULL)

#define TEST_ASSERT_EQUAL_SIZE_T(expected, actual)                             \
    TEST_ASSERT_EQUAL_UINT_MESSAGE((size_t)(expected), (size_t)(actual), NULL)

#define TEST_ASSERT_EQUAL_HEX8(expected, actual)                               \
    TEST_ASSERT_EQUAL_UINT8((expected), (actual))

#define TEST_ASSERT_EQUAL_HEX16(expected, actual)                              \
    TEST_ASSERT_EQUAL_UINT16((expected), (actual))

#define TEST_ASSERT_EQUAL_HEX32(expected, actual)                              \
    TEST_ASSERT_EQUAL_UINT32((expected), (actual))

/* Range and threshold assertions */
#define TEST_ASSERT_INT_WITHIN_MESSAGE(delta, expected, actual, message)       \
    UnityAssertNumbersWithin((int64_t)(delta),                                 \
                             (int64_t)(expected),                              \
                             (int64_t)(actual),                                \
                             (message),                                        \
                             (uint32_t)__LINE__)

#define TEST_ASSERT_INT_WITHIN(delta, expected, actual)                        \
    TEST_ASSERT_INT_WITHIN_MESSAGE((delta), (expected), (actual), NULL)

#define TEST_ASSERT_GREATER_THAN(threshold, actual)                            \
    UnityAssertGreaterOrLess((int64_t)(threshold), (int64_t)(actual), 1, NULL, (uint32_t)__LINE__)

#define TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual)                        \
    UnityAssertGreaterOrLess((int64_t)(threshold), (int64_t)(actual), 2, NULL, (uint32_t)__LINE__)

#define TEST_ASSERT_LESS_THAN(threshold, actual)                               \
    UnityAssertGreaterOrLess((int64_t)(threshold), (int64_t)(actual), -1, NULL, (uint32_t)__LINE__)

#define TEST_ASSERT_LESS_OR_EQUAL(threshold, actual)                           \
    UnityAssertGreaterOrLess((int64_t)(threshold), (int64_t)(actual), -2, NULL, (uint32_t)__LINE__)

/* String and memory assertions */
#define TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, actual, message)            \
    UnityAssertEqualString((const char *)(expected),                           \
                           (const char *)(actual),                             \
                           (message),                                          \
                           (uint32_t)__LINE__)

#define TEST_ASSERT_EQUAL_STRING(expected, actual)                             \
    TEST_ASSERT_EQUAL_STRING_MESSAGE((expected), (actual), NULL)

#define TEST_ASSERT_EQUAL_STRING_LEN_MESSAGE(expected, actual, len, message)   \
    UnityAssertEqualStringLen((const char *)(expected),                        \
                              (const char *)(actual),                          \
                              (size_t)(len),                                   \
                              (message),                                       \
                              (uint32_t)__LINE__)

#define TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len)                    \
    TEST_ASSERT_EQUAL_STRING_LEN_MESSAGE((expected), (actual), (len), NULL)

#define TEST_ASSERT_EQUAL_MEMORY_MESSAGE(expected, actual, len, message)       \
    UnityAssertEqualMemory((const void *)(expected),                           \
                           (const void *)(actual),                             \
                           (size_t)(len),                                      \
                           (message),                                          \
                           (uint32_t)__LINE__)

#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len)                        \
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE((expected), (actual), (len), NULL)

#ifdef __cplusplus
}
#endif

#endif /* UNITY_FRAMEWORK_H */
